#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Mensah's Nokia"
#define WIFI_PASSWORD "lucille2"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCpAx3kmhAhXsR-XS8Y9WqMqbN_TrVtLW0"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://water-level-monitor-98f48-default-rtdb.firebaseio.com/"

/* Defining the pins */
#define LCD_SCL D1
#define LCD_SDA D2
#define UTS_TRIGGER D5
#define RELAY D3
#define UTS_ECHO D6
#define GRN_LED D7
#define RED_LED D8
#define MAX_DISTANCE 200

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String lvlPath = "/level";
String timePath = "/timestamp";

// Parent Node (to be updated in every loop)
String parentPath;

FirebaseJson json;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variable to save current epoch time
int timestamp;
// Delay for the loop
int delay_time = 10000;
// Minimum level to start refilling the tank
float minimum_level = 30;

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 180000;

// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

// Function Prototypes
float readLevel();
void forwardData(float data);
void refill(float min_level);

LiquidCrystal_I2C lcd(0x27, 16, 2); // Setting up the LCD

void setup() {
  // Setting baud for serial communication
  Serial.begin(115200);

  // Pin declarations
  pinMode(UTS_TRIGGER, OUTPUT);
  pinMode(UTS_ECHO, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GRN_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);

  // Setting up the LCD
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

  initWiFi();                                      // Setup to connect wiFi
  timeClient.begin();                              // Create timestamp

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Update database path
  databasePath = "/readings";
}

void loop() {
  if (readLevel() < minimum_level) {
    refill(readLevel());
  }
  forwardData(readLevel()); //posting to the database
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

void forwardData(float data) {
  // Send new readings to database
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    Serial.print ("time: ");
    Serial.println (timestamp);

    parentPath = databasePath + "/" + String(timestamp);

    json.set(lvlPath.c_str(), String(data));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
}

void refill(float min_level) {
  lcd.clear();
  lcd.print("Water Refilling");
  while (min_level != MAX_DISTANCE) {
    digitalWrite(RELAY, HIGH);
  }
}
