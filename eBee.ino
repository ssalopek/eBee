#include <HX711.h> 0996838149
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Http.h>

#define DELAY_TIME 10000

// DHT pin values and other
#define DHTPIN 6 //DHT pin
#define DHTTYPE DHT11 //DHT sensor type

// Weight senzor pin values
#define DOUT 2 //load cell DT pin
#define CLK 3 //load cell SCK pin

// GSM pins and other values
const char BEARER[] PROGMEM = "internet.ht.hr";
#define RST_PIN 8
#define RX_PIN 10
#define TX_PIN 9

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

  char sensorData[256];
  serializeJson(doc, sensorData);
  
  // send Post request to Sensor api route
  postSensorData(sensorData);

  // display data on lcd
  showReadings(doc["Weight"], doc["Temperature"], doc["Humidity"]);

  // turn off components to preserve power
  scale.power_down();

  // sleep time - power down before doing another sensor reading and posting to database
  delay(DELAY_TIME);

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
HTTP connectHttp(int *success)
{
  HTTP http(9600, RX_PIN, TX_PIN, RST_PIN);
  result = http.connect(BEARER);
  Serial.print(F("HTTP connect: "));
  Serial.println(result); // result bi trebo biti 0 što znači SUCCESS po njegovim Enumima
	
  if (result == SUCCESS)
  {
    Serial.println(response);
  	*success = 0;
  }
  else
  {
	Serial.println(F("Failed to connect."));
	*success = 1;
  	delay(1000);
  }
	
  return http;
}

void disconnectHttp(HTTP http)
{
  Serial.print(F("HTTP disconnect: "));
  Serial.print(http.disconnect());
}

void postSensorData(char* body)
{
  int success = 1;
  do
  {
  	HTTP http = connectHttp(&success);
  }while(success == 1);
  
  char response[58];
  result = http.post("https://iotebee.azurewebsites.net/api/SensorData", body, response);
  Serial.print(F("HTTP POST: "));
  Serial.println(result);
  if (result == SUCCESS)
  {
    Serial.println(response);
  }
  
  disconnectHttp(http);
  return;
}

// sends a post request which will add this sensor to the Sensor Database Table and return the Id of the sensor
// can return empty string "" or the Id of the sensor in the table
// the Id should be displayed on the LCD screen
String postSensor()
{
  	
  int success = 1;
  do
  {
  	HTTP http = connectHttp(&success);
  }while(success == 1);
  
  char response[58]; // očekujemo 58 charactera da dobijemo nazad od servera
  const char* body = "";
  result = http.post("https://iotebee.azurewebsites.net/api/Sensor", body, response);
  Serial.print(F("HTTP POST: "));
  Serial.println(result);
  if (result == SUCCESS)
  {
    Serial.println(response);
    StaticJsonBuffer<58> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(response);

    String id = root[F("SensorId")];
    Serial.print(F("ID: "));
    Serial.println(id);

    disconnectHttp(http);
    return id;
  }
  
  disconnectHttp(http);
  return ""
  // 3. Read the Json response containing the SensorId and deserialize the json
  //const size_t capacity = JSON_OBJECT_SIZE(1) + 60;
  //DynamicJsonDocument doc(capacity);
  
  //deserializeJson(doc, client);
  
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
