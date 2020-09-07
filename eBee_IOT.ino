#include <HX711.h> 0996838149
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define DHTTYPE DHT11 //DHT sensor type
/******** PINS *******/
#define DOUT 2 //load cell DT pin
#define CLK 3 //load cell SCK pin
//#define BTN_SCREEN 4 //button for changing LCD screen
#define DHTPIN 6 //DHT pin
#define SIM800_TX_PIN 10 //GSM TX pin
#define SIM800_RX_PIN 11 //GSM RX pin
#define GSM_RESET 4
#define STATUS 13

//create software serial object to communicate with SIM800
SoftwareSerial gsm(SIM800_TX_PIN,SIM800_RX_PIN);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); //LCD address -> 0x27, 16 chars, 2 line display
HX711 scale; 

String phoneNumber = "+385989393229"; 
String webPage = "https://ebee.000webhostapp.com/addData.php";
//int buttonPin = 2;

float temp, hum;
float calibration_factor = 782.4; //load cell calibration factor
//int lcdScreen = 0; //first lcd screen 
//int maxScreens = 2; //max number of multiple switchable screens
//unsigned long lastScreenRefresh = 0;
//unsigned long screenRefreshDelay = 500;
//int buttonState = 0;

void setup() {
  Serial.begin(9600); 
 // gsmStatus();
  //pinMode(BTN_SCREEN, INPUT);
  scale.begin(DOUT, CLK); //initialize HX711 library with data output pin and clock input pin 
  scale.set_scale(calibration_factor); //set the SCALE value that is used to convert the raw data to "human readable" data (measure units)
  scale.tare(); //set the OFFSET value for tare weight
  dht.begin(); //initalize DHT
  lcd.init(); // initialize LCD
  lcd.backlight(); //turn ON backlight
}

void loop() {
  //loadCell();
  //readTemp();
  //readHum();

  char buf[20];
  float data = prepareData();
  //dtostrf(floatvar, length of the string that will be created,  number of digits after the deimal point to print, array to store the results)
  dtostrf(data, 3, 1, buf); //convert a float to a char array 

  String message = String("temp=");
  message += buf;

  gsm_fullPower();
  //toCharArray() -> Copies the String’s characters to the supplied buffer
  message.toCharArray(buf, 20); 

  internet(buf);
  resetGsm();
  gsm_lowPower();
}

float prepareData(){
  float temp = 0;
  for(int i=0; i<30; i++)
  {
    float tempData = dht.readTemperature();
    temp += tempData;
    delay(2000);
  }
  return temp;
  }

void loadCell(){
  scale.set_scale(calibration_factor);
  float masa = scale.get_units(); //returns get_value() divided by SCALE, that is the raw value divided by a value obtained via calibration
  if(masa < 0.0){
    masa = 0.0;
    }
  lcd.setCursor(0, 0);
  lcd.print("Masa|" );
  lcd.setCursor(0,1);

  lcd.print(masa); 
  lcd.setCursor(3,1);
  lcd.print("|g");
  }
void readTemp() {
  temp = dht.readTemperature();
  lcd.setCursor(5, 0);
  lcd.print("Temp|");
  lcd.setCursor(5,1);
  lcd.print(temp);
  lcd.print((char)223); //degree symbol
  lcd.print("C");
}

void readHum() {
  hum = dht.readHumidity();
  lcd.setCursor(10, 0);
  lcd.print("Vlaga|");
  lcd.setCursor(10,1);
  lcd.print((String)hum + "%");
}

void sms(){
  gsm.println("AT+CMGF=1");
  delay(1000);
  gsm.print("AT+CMGS=");
  gsm.print('"');
  gsm.print(phoneNumber);
  gsm.println('"');
  delay(1000);
  gsm.write(26);
  }

void connectToNet(){ 
  gsm.println("AT+CGATT=1"); //Perform a GPRS Attach. The device should be attached to the GPRS network before a PDP context can be established 
  delay(1000);
  gsm.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""); //Configure bearer profile
  delay(1000);
  gsm.println("AT+SAPBR=3,1,\"APN\",\"internet.ht.hr\""); //setup dedicated APN
  delay(1000);
  gsm.println("AT+SAPBR=1,1"); //open a GPRS context
  delay(3000);
  }

void disconnectFromNet(){
  gsm.println("AT+SAPBR=0,1"); //close GPRS context
  delay(1000);
  }

void internet(char *message){
  connectToNet();
  gsm.println("AT+HTTPINIT"); //init HTTPS service
  delay(1000);
  gsm.println("AT+HTTPPARA=\"CID\",1"); //set parameters for HTTPS session
  delay(1000);
  gsm.print("AT+HTTPPARA=\"URL\",\"");
  gsm.print(webPage);
  gsm.print(message);
  delay(1000);
  gsm.println("AT+HTTPACTION=2"); //start GET session (requset header only to reduce data usage)
  delay(3000);
  gsm.println("AT+HTTPTERM"); //terminate HTTPS service
  disconnectFromNet();
  delay(10000);
  }

void resetGsm()
{
  digitalWrite(STATUS, 1);
  digitalWrite(GSM_RESET, 0);
  delay(100);
  digitalWrite(GSM_RESET, 1);
  digitalWrite(STATUS, 0);
  delay(59000);  
}

void gsm_lowPower(){
  gsm.println("AT+CFUN=0,1"); //minimum functionality
  delay(60000);
  }

void gsm_fullPower(){
  gsm.println("AT+CFUN=1,1"); //full functionality
  delay(60000);
  }
/*
void gsmStatus(){
  gsm.begin(9600); //begin serial communication with Arduino and SIM800
  delay(1000);
  gsm.println("AT");
  delay(1000);
  Serial.println("Jakost signala (0-31):");
  gsm.println("AT+CSQ");
  delay(1000);
  gsm.println("AT+CREG?");
  delay(1000);
}*/
