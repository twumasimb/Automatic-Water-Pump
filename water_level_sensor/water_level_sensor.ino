#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Esp.h>
#include <cmath>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/* Defining the pins */
#define LCD_SCL D1
#define LCD_SDA D2
#define UTS_TRIGGER D5
#define RELAY D3
#define UTS_ECHO D6
#define GRN_LED D7
#define RED_LED D8
#define MAX_DISTANCE 200
#define API_KEY "AIzaSyCpAx3kmhAhXsR-XS8Y9WqMqbN_TrVtLW0";
#define DATABASE_URL "https://water-level-monitor-98f48-default-rtdb.firebaseio.com/";
// Password setup
const char *ssid = "Mensah's Nokia";
const char *password = "lucille2";
int timestamp;
String path;

// Set time by adjusting this. Time is in milliseconds
int delay_time = 10000;
float minimum_level = 30;
unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

WiFiClient client; // WiFi setup 
LiquidCrystal_I2C lcd(0x27, 16, 2); // Setting up the LCD

/* Funtion prototypes */
float readLevel();
void checkInternet();
void sendData(float);
void refill(float);
void fireSend(float);
unsigned long getTime();

void setup()
{
  // Pin declarations
  pinMode(UTS_TRIGGER, OUTPUT);
  pinMode(UTS_ECHO, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GRN_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);

  Serial.begin(9600);
  /***************
   * LCD display *
   * https://console.firebase.google.com/project/water-level-monitor-98f48/database/water-level-monitor-98f48-default-rtdb/data/~2F
   *API Key = AIzaSyCpAx3kmhAhXsR-XS8Y9WqMqbN_TrVtLW0
   * *************/

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.print("Hello Welcome");
  delay(1500);
  // Setting the relay to off position before the loop
  digitalWrite(RELAY, LOW);
  Serial.println("Relay is off");
  delay(2000);
  
  // The system will work only if the WiFi connection is available
  checkInternet(); // connect to wifi

    /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  if (readLevel() < minimum_level){
    refill(readLevel());
  }
  fireSend(readLevel()); //posting to the database
  delay(delay_time);
}

float readLevel()
{
  //sending the signal
  digitalWrite(UTS_TRIGGER, LOW);
  delayMicroseconds(5);
  digitalWrite(UTS_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(UTS_TRIGGER, LOW);
  float duration = pulseIn(UTS_ECHO, HIGH);  // Receiving the signal
  float cm = (duration / 2) / 29.1; // calculating the distance in centimeters
  float water_level = MAX_DISTANCE - cm; //level of oil
  lcd.setCursor(0, 1);
  lcd.print("level: ");
  lcd.print(water_level);
  lcd.print(" cm");
  return water_level;
}

void sendData(float lvl)
{
  // Creating json data
  StaticJsonBuffer<300> JSONbuffer; //Declaring static JSON buffer
  JsonObject &JSONencoder = JSONbuffer.createObject();

  //Encoding data
  JSONencoder["level"] = lvl;

  char JSONmessageBuffer[300];
  JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println(JSONmessageBuffer);

  // Declaring object class of  the HTTPClient
  HTTPClient http;

  http.begin(client, "https://www.thunderclient.com/welcome");
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(JSONmessageBuffer); //Send the request
  String payload = http.getString();           //Get the response payload

  Serial.println(httpCode); //Print HTTP return code
  if (httpCode == 200)
  {
    for (int x = 0; x < 5; x++)
    {
      digitalWrite(GRN_LED, HIGH);
      delay(500);
      digitalWrite(GRN_LED, LOW);
      delay(500);
    }
  }
  http.end(); //Close connection
}
void checkInternet()
{
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.scrollDisplayLeft();
  lcd.print("Connecting to ");
  lcd.print(ssid);
  lcd.println(" ...");

  // Waiting for the Wifi to connect
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print(++i);
    lcd.print(' ');
  }
  lcd.clear();
  lcd.scrollDisplayLeft();
  lcd.print("Connection established!");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  WiFi.mode(WIFI_STA);
  delay(2500); 
}

void refill(float min_level){
  lcd.clear();
  lcd.print("Water Refilling");
  while(min_level != MAX_DISTANCE){
    digitalWrite(RELAY, HIGH);
  }
}

void fireSend(float water_level){
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    timestamp = getTime();
    path = String(timestamp) + "/level";
    // Write an Int number on the database path test/int
    if (Firebase.RTDB.setInt(&fbdo, path, water_level)){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}
