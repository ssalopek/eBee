#include <HX711.h> 0996838149
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <GSM.h>
#include <ArduinoJson.h>

#define DELAY_TIME 10000

// DHT pin values and other
#define DHTPIN 6 //DHT pin
#define DHTTYPE DHT11 //DHT sensor type

// Weight senzor pin values
#define DOUT 2 //load cell DT pin
#define CLK 3 //load cell SCK pin

// GSM pins and other values
#define PINNUMBER "" // PIN of sim card
#define GPRS_APN "internet.ht.hr"
#define GPRS_LOGIN ""
#define GPRS_PASSWORD ""

// Variables for GSM
GSMClient client;
GPRS gprs;
GSM gsmAccess;
GSM_SMS sms;

char url[] = "https://localhost:44312"; // ovo treba zamjeniti sa URL-om stanice kad se podigne

// DHT sensor variables
DHT dht(DHTPIN, DHTTYPE);
// Weight sensor variables
HX711 scale;
// load cell calibration factor 
// obtained by calibrating the scale with known weights
float calibration_factor = 782.4;

// LCD display variables
LiquidCrystal_I2C lcd(0x27, 16, 2); //LCD address -> 0x27, 16 chars, 2 line display
// stanje prikaza na lcd-u, bilo bi dobro addat tipkalo koje mijenja stanje
// state == 0 - prikaz sensorId-a
// state == 1 - prikaz iščitavanja
int lcdState = 0;

//-- general variables --
// recieved once setup finishes making inital POST to web service
// should be stored in local storage (SD card or similar)
String sensorId = ""; 

// refer to https://arduinojson.org/v6/example/generator/ for JSON serialization
// refer to https://arduinojson.org/v6/example/http-client/ for JSON deserialization of HTTP responses from server
// za quick lookup kolko memorije ce trebat za neki json da se de/serializira: https://arduinojson.org/v6/assistant/

// use as startup for setting up values
void setup() {
  Serial.begin(9600); 

  // initialize the GSM module 
  // connected the GSM to the internet (ISP - A1, HT, Tele2 etc.)
  beginGsm();

  // now send a POST request to get SensorId
  while(sensorId.length() <= 0){
    sensorId = postSensor();

    if(sensorId.length() <= 0){
      delay(10000); // wait for 10 seconds before sending another POST request
    }
  }
  // display sensor id
  showSensorId();
  delay(60000); // show sensor Id for the next 60 seconds (better implement switch by button)
  
  // init weight sensor
  scale.begin(DOUT, CLK); //initialize HX711 library with data output pin and clock input pin 
  scale.set_scale(calibration_factor); //set the SCALE value that is used to convert the raw data to "human readable" data (measure units)
  scale.tare(); // resets the scale to 0
  Serial.println("Weight sensor initialized"); 

  // init DHT sensor
  dht.begin(); //initalize DHT
  Serial.println("DHT sensor initialized"); 

  // init LCD screen
  lcd.init(); // initialize LCD
  lcd.backlight(); //turn ON backlight
  Serial.println("LCD initialized"); 
}

// updates again once previous loop is finished
void loop() 
{
  const size_t capacity = JSON_OBJECT_SIZE(3);
  DynamicJsonDocument doc(capacity);
  
  // read sensor values (temperature, humidity, weight)
  // read humidity - takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  doc["Humidity"] = dht.readHumidity();
  // read temperature (in Celsius)
  doc["Temperature"] = dht.readTemperature();
  // read bee hive weight 
  doc["Weight"] = readWeight();
  // add the SensorId
  doc["SensorId"] = sensorId;

  // Check if any reads failed and exit early (to try again).
  if (isnan(doc["Humidity"]) || isnan(doc["Temperature"]) || isnan(doc["Weight"])) {
    Serial.println("Failed to read from sensors!");
    delay(DELAY_TIME);
    return;
  }

  String sensorData = "";
  serializeJson(doc, sensorData);
  
  // send Post request to Sensor api route
  postSensorData(sensorData);

  // display data on lcd
  showReadings(doc["Weight"], doc["Temperature"], doc["Humidity"]);

  // turn off components to preserve power
  shutdownGsm();
  scale.power_down();

  // sleep time - power down before doing another sensor reading and posting to database
  delay(DELAY_TIME);

  beginGsm();
  scale.power_up();
}

// funkcije senzora za masu
float readWeight()
{
  float weight = scale.get_units(); // reading minus tare weight divided by SCALE parameter
  if(weight < 0.0){
    weight = 0.0;
  }

  return weight;
}

// funkcije za GSM i komunikaciju s web servisom
void beginGsm()
{
  Serial.println("Starting Arduino web client");
  boolean connected = true;
  while(connected){
      if((gsmAccess.begin(PINNUMBER)==GSM_READY)&&
          (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD)==GPRS_READY)){
            connected = false;}
      else{
          Serial.println("Not connected.");
          delay(1000);
      }
  }
  Serial.println("GSM initialized"); 
}

void shutdownGsm()
{
  gsmAccess.shutdown();
}

void postSensorData(String data)
{
  if (client.connect(url,80)) {
    Serial.println("Connecting...");
    client.println("POST /api/SensorData HTTP/1.1"); // request and route
    client.print("Host: "); // the Host path
    client.println(url);
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.println(data);
    }
  else {
    Serial.println("Connection failed.");
    Serial.println();
    Serial.println("Disconnecting.");
    client.stop();
  }

  boolean status = checkStatusOk();
  if(!status){
    Serial.println("Failed to POST sensor data to service database.");
  }

  Serial.println("Succeeded in POSTing sensor data.");
  client.stop();
}

// sends a post request which will add this sensor to the Sensor Database Table and return the Id of the sensor
// can return empty string "" or the Id of the sensor in the table
// the Id should be displayed on the LCD screen
String postSensor()
{
  // send POST request for SensorId to web service
  if (client.connect(url,80)) {
    Serial.println("Connecting...");
    client.println("POST /api/Sensor HTTP/1.1"); // request and route
    client.print("Host: "); // the Host path
    client.println(url);
    client.println("Connection: close"); // ili keep-alive
    client.println("Content-Type: application/json");
    client.println("Content-Length: 0");
    }
  else {
    Serial.println("Connection failed.");
    Serial.println();
    Serial.println("Disconnecting.");
    client.stop();
  }

  // read the response step by step (3 steps)
  // 1. Check HTTP status
  boolean status = checkStatusOk();
  if(!status){
    return "";
  }

  // 2. Skip HTTP headers
  buffer = "";
  while(client.available() || client.connected()) // available vraća broj bitova dostupnih za čitanje
  {
    // read one character from buffer
    char c = client.read();
    buffer += c;
    Serial.print(c);
    
    if(buffer.indexOf("\r\n\r\n") > 0)
    {
      break;
    }
  }

  // 3. Read the Json response containing the SensorId and deserialize the json
  const size_t capacity = JSON_OBJECT_SIZE(1) + 60;
  DynamicJsonDocument doc(capacity);
  
  deserializeJson(doc, client);
  client.stop(); // stop client connection, we don't need it anymore
  
  return doc["SensorId"];
}

bool checkStatusOk()
{
  String buffer = "";
  while(client.available() || client.connected()) // available vraća broj bitova dostupnih za čitanje
  {
    // read one character from buffer
    char c = client.read();
    buffer += c;
    Serial.print(c);
    
    // check if we reached carriage return
    // if we did then check if we have recieve 200 OK status
    if(c == '\r'){
      if (buffer.indexOf("HTTP/1.1 200 OK") > 0) {
        Serial.print("Unexpected response: ");
        Serial.println(buffer);
        return false;
      }

      return true;
    }
  }

  Serial.println("Could not find the request status or response wasn't recieved might want to add delay (of 1 or more seconds) before while loop.");
  return false;
}

// funkcije za prikaz na LCD-u
void showSensorId()
{
  // first we need to clear the screen
  lcd.clear();

  lcd.autoscroll();
  lcd.print(sensorId);
  lcd.noAutoscroll();
}

void showReadings(float masa, float temp, float hum)
{
  // first we need to clear the screen
  lcd.clear();
  
  // weight readings
  lcd.setCursor(0, 0);
  lcd.print("Masa|" );
  lcd.setCursor(0,1);
  lcd.print(masa); 
  lcd.setCursor(3,1);
  lcd.print("|g");

  // temp readings
  lcd.setCursor(5, 0);
  lcd.print("Temp|");
  lcd.setCursor(5,1);
  lcd.print(temp);
  lcd.print((char)223); //degree symbol
  lcd.print("C");

  // humidity readings
  lcd.setCursor(10, 0);
  lcd.print("Vlaga|");
  lcd.setCursor(10,1);
  lcd.print((String)hum + "%");
}
