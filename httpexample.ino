#include <Ftp.h>
#include <Geo.h>
#include <GPRS.h>
#include <Http.h>
#include <Parser.h>
#include <Result.h>
#include <Sim800.h>

#include <ArduinoJson.h>
#include <Http.h>
#include <DHT.h>
#include <HX711.h>

#define DOUT 2 //load cell DT pin
#define CLK 3 //load cell SCK pin
#define DHTPIN 6 //DHT pin
#define DHTTYPE DHT11 //DHT sensor type
#define RST_PIN 8
#define RX_PIN 10
#define TX_PIN 9

// GSM pins and other values
#define PINNUMBER "" // PIN of sim card
#define GPRS_APN "internet.ht.hr"
#define GPRS_LOGIN ""
#define GPRS_PASSWORD ""
#define DELAY_TIME 10000

const char BEARER[] PROGMEM = "internet.ht.hr";
DHT dht(DHTPIN, DHTTYPE);
HX711 scale;
SoftwareSerial GSM_serial(RX_PIN, TX_PIN);
String sensorId = "";

// the setup routine runs once when you press reset:
void setup()
{
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Starting!");
}

// the loop routine runs over and over again forever:
void loop()
{
  HTTP http(9600, RX_PIN, TX_PIN, RST_PIN);

  char response[32];
  char body[90];
  Result result;

  // Notice the bearer must be a pointer to the PROGMEM
  result = http.connect(BEARER);
  Serial.print(F("HTTP connect: "));
  Serial.println(result);

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

  sprintf(body, "{\"title\": \"%s\", \"body\": \"%s\", \"user_id\": \"%d\"}", "Arduino", "Test", 1);
  result = http.post("https://iotebee.azurewebsites.net/api/Sensor", body, response);
  Serial.print(F("HTTP POST: "));
  Serial.println(result);
  if (result == SUCCESS)
  {
    Serial.println(response);
    StaticJsonDocument<64> jsonBuffer;
    deserializeJson(jsonBuffer, response);
    postData(response);

  }
  Serial.print(F("HTTP disconnect: "));
  Serial.print(http.disconnect());
}

void connect()
{
  GSM_serial.print("AT+CGATT=1\r\n");
  delay(1000);
  GSM_serial.print("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
  delay(1000);
  GSM_serial.print("AT+SAPBR=3,1,\"APN\",\"");
  GSM_serial.print(BEARER);
  GSM_serial.print("\"\r\n");
  delay(1000);
  GSM_serial.print("AT+SAPBR=1,1\r\n");
  delay(3000);
}

void postData(char *message)
{
  connect();
  GSM_serial.print("AT+HTTPINIT\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPPARA=\"CID\",1\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPPARA=\"URL\",\"https://iotebee.azurewebsites.net/api/Sensor\"\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPPARA=\"CONTENT\", \"application/json\"\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPDATA=\"");
  GSM_serial.print(message);
  GSM_serial.print("\",10000\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPSSL=1\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPACTION=1\r\n"); // Request POST 
  delay(3000);
  GSM_serial.print("AT+HTTPREAD\r\n");
  delay(1000);
  GSM_serial.print("AT+HTTPTERM\r\n");
  GSM_serial.print("AT+SAPBR=0,1\r\n");
  delay(45000); 
}


float readWeight()
{
  float weight = scale.get_units(); // reading minus tare weight divided by SCALE parameter
  if(weight < 0.0){
    weight = 0.0;
  }

  return weight;
}
