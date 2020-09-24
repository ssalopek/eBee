#include <HX711.h> 0996838149
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "SIM800L.h"

#define DELAY_TIME 10000

// DHT pin values and other
#define DHTPIN 6 //DHT pin
#define DHTTYPE DHT11 //DHT sensor type

// Weight senzor pin values
#define DOUT 2 //load cell DT pin
#define CLK 3 //load cell SCK pin

// GSM pins and other values
SIM800L* sim800l;
#define TX_PIN 9
#define RX_PIN 10
#define RST_PIN 8
const char APN[] = "internet.ht.hr";
const char URL_SensorData[] = "https://iotebee.azurewebsites.net/api/SensorData";
const char URL_Sensor[] = "https://iotebee.azurewebsites.net/api/Sensor";
const char CONTENT_TYPE[] = "application/json";

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
char* sensorId; 

// refer to https://arduinojson.org/v6/example/generator/ for JSON serialization
// refer to https://arduinojson.org/v6/example/http-client/ for JSON deserialization of HTTP responses from server
// za quick lookup kolko memorije ce trebat za neki json da se de/serializira: https://arduinojson.org/v6/assistant/

// use as startup for setting up values
void setup() {
  Serial.begin(9600); 
  while(!Serial);

  // Initialize a SoftwareSerial
  SoftwareSerial* serial = new SoftwareSerial(TX_PIN, RX_PIN);
  serial->begin(9600);
  delay(1000);

  // Initialize SIM800L driver with an internal buffer of 200 bytes and a reception buffer of 512 bytes, debug disabled
  //sim800l = new SIM800L((Stream *)serial, RST_PIN, 200, 512);

  // Equivalent line with the debug enabled on the Serial
  sim800l = new SIM800L((Stream *)serial, RST_PIN, 200, 512, (Stream *)&Serial);

  // Setup module for GPRS communication
  setupModule();

  // now send a POST request to get SensorId
  while(!postSensor()){
    delay(10000); // wait for 10 seconds before sending another POST request   
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
void setupModule() {
    // Wait until the module is ready to accept AT commands
  while(!sim800l->isReady()) {
    Serial.println(F("Problem to initialize AT command, retry in 1 sec"));
    delay(1000);
  }
  Serial.println(F("Setup Complete!"));

  // Print version
  Serial.print("Module ");
  Serial.println(sim800l->getVersion());
  Serial.print("Firmware ");
  Serial.println(sim800l->getFirmware());

  // Wait for the GSM signal
  uint8_t signal = sim800l->getSignal();
  while(signal <= 0) {
    delay(1000);
    signal = sim800l->getSignal();
  }
  Serial.print(F("Signal OK (strenght: "));
  Serial.print(signal);
  Serial.println(F(")"));
  delay(1000);

  // Wait for operator network registration (national or roaming network)
  NetworkRegistration network = sim800l->getRegistrationStatus();
  while(network != REGISTERED_HOME && network != REGISTERED_ROAMING) {
    delay(1000);
    network = sim800l->getRegistrationStatus();
  }
  Serial.println(F("Network registration OK"));
  delay(1000);

  // Setup APN for GPRS configuration
  bool success = sim800l->setupGPRS(APN);
  while(!success) {
    success = sim800l->setupGPRS(APN);
    delay(5000);
  }
  Serial.println(F("GPRS config OK"));
}

bool connectGprs()
{
  // Go into low power mode
  bool normalPowerMode = sim800l->setPowerMode(NORMAL);
  if(normalPowerMode) {
    Serial.println(F("Module in normal power mode"));
  } else {
    Serial.println(F("Failed to switch module to normal power mode"));
  }
  
  // Establish GPRS connectivity (5 trials)
  bool connected = false;
  for(uint8_t i = 0; i < 5 && !connected; i++) {
    delay(1000);
    connected = sim800l->connectGPRS();
  }

  // Check if connected, if not reset the module and setup the config again
  if(connected) {
    Serial.println(F("GPRS connected !"));
  } else {
    Serial.println(F("GPRS not connected !"));
    Serial.println(F("Reset the module."));
    sim800l->reset();
    setupModule();
  }
  
  return connected;
}

bool disconnectGprs()
{
  // Close GPRS connectivity (5 trials)
  bool disconnected = sim800l->disconnectGPRS();
  for(uint8_t i = 0; i < 5 && !disconnected; i++) {
    delay(1000);
    disconnected = sim800l->disconnectGPRS();
  }
  
  if(disconnected) {
    Serial.println(F("GPRS disconnected !"));
  } else {
    Serial.println(F("GPRS still connected !"));
  }

  // Go into low power mode
  bool lowPowerMode = sim800l->setPowerMode(MINIMUM);
  if(lowPowerMode) {
    Serial.println(F("Module in low power mode"));
  } else {
    Serial.println(F("Failed to switch module to low power mode"));
  }

  return disconnected;
}

void postSensorData(char *body)
{
  while(!connectGprs());

  Serial.println(F("Start HTTP POST..."));

  // Do HTTP POST communication with 10s for the timeout (read and write)
  uint16_t rc = sim800l->doPost(URL_SensorData, CONTENT_TYPE, *body, 10000, 10000);
  if(rc == 200) {
    // Success, output the data received on the serial
    Serial.print(F("HTTP POST successful ("));
    
    uint8_t bytesRecv = sim800l->getDataSizeReceived()
    Serial.print(bytesRecv);
    
    Serial.println(F(" bytes)"));
    Serial.print(F("Received : "));

    const char* dataRecv = sim800l->getDataReceived();
    Serial.println(dataRecv);
  } else {
    // Failed...
    Serial.print(F("HTTP POST error "));
    Serial.println(rc);
  }
  
  while(!disconnectGprs());
  
  return;
}

// sends a post request which will add this sensor to the Sensor Database Table and return the Id of the sensor
// can return empty string "" or the Id of the sensor in the table
// the Id should be displayed on the LCD screen
bool postSensor()
{ 
  while(!connectGprs());

  Serial.println(F("Start HTTP POST..."));

  bool postSuccess = false;
  // Do HTTP POST communication with 10s for the timeout (read and write)
  uint16_t rc = sim800l->doPost(URL_Sensor, CONTENT_TYPE, "", 10000, 10000);
  if(rc == 200) {
    // Success, output the data received on the serial
    Serial.print(F("HTTP POST successful ("));
    
    uint8_t bytesRecv = sim800l->getDataSizeReceived()
    Serial.print(bytesRecv);
    
    Serial.println(F(" bytes)"));
    Serial.print(F("Received : "));

    const char* dataRecv = sim800l->getDataReceived();
    Serial.println(dataRecv);

    const size_t capacity = JSON_OBJECT_SIZE(1) + 60;
    DynamicJsonDocument doc(capacity);
    
    deserializeJson(doc, dataRecv);

    sensorId = doc["SensorId"];
    
    Serial.print(F("ID: "));
    Serial.println(sensorId);
    postSuccess = true;
  } else {
    // Failed...
    Serial.print(F("HTTP POST error "));
    Serial.println(rc);
  }
  
  while(!disconnectGprs());

  return postSuccess;
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
