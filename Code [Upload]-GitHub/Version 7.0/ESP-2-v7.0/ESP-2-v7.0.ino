/*
  Hello This is me Hashtag .....
  This is my Personal Home Automation Work,
  I use this project in my Home for sensing my Room Enviroment
  and receive data ESP to ESP's.
  The data is displayed on local LCD and send to other ESP.
  This Code also makes sure the ESP2 and Wifi stays connected or retry...!
  It shows appropriate data and Alert display for any Gas leakage..!!

  Name: Aniket Chowdhury [Hashtag]
  Email: micro.aniket@gmail.com
  GitHub: https://github.com/itzzhashtag
  Instagram: https://instagram.com/itzz_hashtag
  LinkedIn: https://www.linkedin.com/in/itzz-hashtag/
*/

//Open File->Preferences->Additional Boards Manager URL's ->(Copy and Paste) "https://dl.espressif.com/dl/package_esp32_index.json,http://arduino.esp8266.com/stable/package_esp8266com_index.json"
//ALso Change Values and Data before using the code (Changes needed)
//  [Calibrate the values according to your Data Available]

//=====================================================
// --- Libraries Used ---
//=====================================================
#include <WiFi.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//=====================================================
// --- Network Details and Constants ---
//=====================================================
const char* ssid = "your_ssid";           // Wifi SSID
const char* password = "ssid_password";   // Wifi Passcode
IPAddress local_IP("Your_ESP3_IP");       // Static IP for ESP1
IPAddress gateway("Your_static_Ip");      // Routers IP - Wifi
IPAddress subnet(255, 255, 255, 0);

const char* esp1_ip = "Your_ESP1_IP";     // ESP1 IP
const char* esp2_ip = "Your_ESP2_IP";     // ESP2 IP
const char* esp3_ip = "Your_ESP3_IP";     // ESP3 IP
const int esp1_port = 83;                 // ESP1 Server Port
const int esp2_port = 81;                 // ESP2 Server Port
const int esp3_port = 82;                 // ESP3 Server Port

WiFiServer server(esp2_port);

//=====================================================
// --- Matrix Pins ---
//=====================================================
#define DHTPIN 32                         // what pin DHT22 is connected to  
#define DHTTYPE DHT22
const int ONE_WIRE_BUS = 4;
const int MQ7sensor = 35;                 // MQ7 Sensor Input
const int MQ135sensor = 34;               // MQ135 Sensor Input
const int led_R = 5;                      // Green Led Input
const int led_G = 18;                     // Red Led Input
const int led_Y = 19;                     // Yellow Led Input
const int Buzz = 23;                      // Buzzer Input

//===============================================
//--- Global State Variables ---
//===============================================
hd44780_I2Cexp lcd;                       // Initialize LCD Matrix to (20x4)
DHT dht(DHTPIN, DHTTYPE);                 // Initialize DHT sensor
OneWire oneWire(ONE_WIRE_BUS);            // Setup a oneWire instance
DallasTemperature sensors(&oneWire);      // Pass oneWire reference to Dallas Temperature sensor
#define CHANNEL 0                         // LEDC_CHANNEL no
int h = 0, t1 = 0;                        // Stores Humidity and Temperature Values[int]
float t = 0, t2 = 0;                              // Stores Temperature value
int ppm = 0;                              // Stores AoQ value
int mq7Val = 0;                           // Stores MQ7 value
int mq5Val = 10;                          // Stores MQ5 value
int mq135Val = 0;                         // Stores MQ135 value
int counter = 1, x = 0, y = 0 ;         // Counters
int flag1 = 0, flag2 = 0, flag3 = 0, flag4 = 0;   // Flag VariablesLeakage
int th1, th2, th3, th4, th5, th6;         // Threshold Adjusters
int mq5Alrm = 0;                          // Alarm Status True/False From ESP-1
const unsigned long TEN_MINUTES = 10 * 60000;       // 10 minutes in milliseconds
unsigned long firstBuzzTime = 0;    // When the first buzz in a window happened
unsigned long lastBuzzTime = 0;     // When the last buzz happened
unsigned long cooldownStart = 0;    // When cooldown started
int buzzCount = 0;                  // Number of buzzes in current 10-min window
bool firstBuzzDone = false;

int mq135_base = 0;
int mq7_base = 0;
bool calibrated = false;

//======================================================
// --- Custum Character for LCD ---
///=====================================================
byte Skull[8] = {B00000, B01110, B10101, B10101, B11111, B01010, B01110, B00000,};
byte Heart[8] = {0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000};
byte Bell[8] = {0b00100, 0b01110, 0b01110, 0b01110, 0b11111, 0b00000, 0b00100, 0b00000};
byte st1[8] = {B00011, B00011, B00011, B00011, B00011, B00011, B00011, B00011,};
byte st2[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000,};
//=====================================================
// --- Setup ---
//=====================================================
void setup()
{
  Serial.begin(115200);                    // Serial baud set to 115200
  sensors.begin();
  lcd.begin(20, 4);                        // LCD Matrix set to 20x4
  lcd.setBacklight(255);                   // Turn on backlight
  pinMode(led_R, OUTPUT);                  // led_R set as OUTPUT
  pinMode(led_G, OUTPUT);                  // led_G set as OUTPUT
  pinMode(led_Y, OUTPUT);                  // led_Y set as OUTPUT
  pinMode(Buzz, OUTPUT);                   // Buzzer set as OUTPUT
  digitalWrite(led_R, HIGH);               // Turns Red Led ON
  digitalWrite(led_G, HIGH);               // Turns Green Led ON
  digitalWrite(led_Y, HIGH);               // Turns Yellow Led ON
  digitalWrite(Buzz, HIGH);                // Set Buzzer to OFF State
  pinMode (MQ135sensor, INPUT);            // MQ135 set as INPUT
  pinMode (MQ7sensor, INPUT);              // MQ7 set as INPUT
  dht.begin();
  lcd.createChar(0, Skull);
  lcd.createChar(2, Heart);
  lcd.createChar(4, Bell);
  lcd.createChar(6, st1);
  lcd.createChar(7, st2);
  Wire.begin(21, 22);
  esp_booting();                          // Start The Wifi connection
}
//=====================================================
// --- Loop ---
//=====================================================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)      // Check Wifi each time and retry if not Connected
    esp_booting();
  Sub_Client();                           // Receives Data from ESP3
  if (mq5Alrm == 0)                       // If LPG Normal State
  {
    getval();
    calculateThresholds(t, h);
    Serial.printf("🌀 AoQ: %d | 💧 H: %d%% | 🌡 T1: %.2f°C | T2: %.2f°C | 🛢 MQ7: %d | 🛢 MQ135: %d | 🛢 MQ5: %d |  Alarm: %s \n", ppm, h, t, t2, mq7Val, mq135Val, mq5Val, mq5Alrm ? "true" : "false");
    lcd.setCursor(2, 0);                  // Set cursor of lcd
    lcd.print(" ");
    lcd.write(byte(6));
    lcd.print(" LIVE Stats ");            // Appropriate display for LCD
    //lcd.write(byte(9));
    lcd.write(byte(7));
    lcd.print(" ");
    lcd.setCursor(0, 1);
    lcd.print("Co2 :");
    lcd.print(mq135Val);
    if (mq135Val < 1000)
      lcd.print("    ");
    lcd.setCursor(12, 1);
    lcd.print("Co :");
    lcd.print(mq7Val);
    if (mq7Val < 1000)
      lcd.print(" ");
    lcd.setCursor(0, 2);
    lcd.print("LPG :");
    lcd.print(mq5Val);
    if (mq5Val < 1000)
      lcd.print("   ");
    lcd.setCursor(11, 2);
    lcd.print("AoQ :");
    lcd.print(ppm);
    if (ppm < 1000)
      lcd.print(" ");
    threshold_display();                       // Display AoQ on LCD with ref to ppm
    delay(400);
    Main_Client();                             // Send those Data to ESP1
  }
  else
  {
    digitalWrite(led_R, HIGH);
    digitalWrite(led_G, LOW);
    digitalWrite(led_Y, HIGH);
    lcd.setCursor(4, 0);
    lcd.write(byte(6));
    lcd.print(" WARNING ");
    lcd.write(byte(7));
    lcd.setCursor(0, 1);
    lcd.write(byte(4));
    lcd.print(" LPG Gas : ");
    lcd.print(mq5Val);
    lcd.setCursor(17, 1);
    lcd.print("ppm");
    lcd.setCursor(0, 3);
    lcd.write(byte(0));
    lcd.print("  GAS Leaking..!! ");
    delay(50);
    warningBuzz(3, 300);                       // Buzzer beeps for 3 time with 300 delay
    Main_Client();                             // Sends Data to ESP1
    delay(500);
  }
}

//=====================================================
// --- Get Values ---
//=====================================================
void getval()
{
  // Raw readings
  int raw135 = analogRead(MQ135sensor);
  int raw7   = analogRead(MQ7sensor);

  // 🧠 SIMPLE BASELINE (hardcoded from your real data)
  // You observed:
  // MQ135 ≈ 500–600
  // MQ7   ≈ 1300–1500

  const int base135 = 550;
  const int base7   = 1400;

  // 🔥 Normalize to ~100 baseline
  mq135Val = (raw135 * 100) / base135;
  mq7Val   = (raw7   * 100) / base7;

  // Clamp to avoid crazy spikes
  mq135Val = constrain(mq135Val, 0, 500);
  mq7Val   = constrain(mq7Val, 0, 500);

  // Final AoQ
  ppm = (mq135Val + mq7Val) / 2;

  // Environmental data
  h = (dht.readHumidity()) - 5;
  t2 = (dht.readTemperature()) - 2;

  sensors.requestTemperatures();
  t = sensors.getTempCByIndex(0);

  if (t < -40 || t > 100) t = 0;
  if (h < 0   || h > 100) h = 0;
  //Serial.printf("DEBUG → Cal:%d | Base135:%d | Base7:%d\n", calibrated, mq135_base, mq7_base);
}
//=====================================================
// --- AOQ Display with Threshold ---
//=====================================================


void calculateThresholds(float temp, float hum)
{
  const float baseTemp = 23.0;
  const float baseHum  = 48.0;

  const float tempCoeff = 0.012;
  const float humCoeff  = 0.006;

  float tempFactor = 1.0 + (temp - baseTemp) * tempCoeff;
  float humFactor  = 1.0 + (hum - baseHum) * humCoeff;  

  float envFactor = (tempFactor + humFactor) / 2.0;

  // 🔥 Clamp to avoid extreme scaling
  envFactor = constrain(envFactor, 0.8, 1.3);

  int thMax = 250 * envFactor;

  th1 = 0;
  th2 = int(0.15 * thMax);
  th3 = int(0.30 * thMax);
  th4 = int(0.50 * thMax);
  th5 = int(0.70 * thMax);
  th6 = thMax;

  Serial.printf("ENV: %.2f | Thresholds => [%d %d %d %d %d %d]\n",envFactor, th1, th2, th3, th4, th5, th6);
}

void threshold_display()
{
  if (ppm > th1 && ppm <= th2)
  {
    flag2 = 0;
    flag3 = 0;
    lcd.setCursor(0, 3);
    lcd.print(" AoQ Lvl: Excellent");
    lcd.write(byte(2));
    Serial.println("AQ Level Excellent");
    digitalWrite(led_R, LOW);
    digitalWrite(led_G, HIGH);
    digitalWrite(led_Y, LOW);
  }
  else if (ppm > th2 && ppm <= th3)
  {
    flag2 = 0;
    flag3 = 0;
    lcd.setCursor(0, 3);
    lcd.print ("  AoQ Level Good ");
    lcd.write(byte(2));
    lcd.print("  ");
    Serial.println("AQ Level Good");
    digitalWrite(led_R, LOW);
    digitalWrite(led_G, HIGH);
    digitalWrite(led_Y, LOW);
  }
  else if (ppm > th3 && ppm <= th4)
  {
    flag2 = 0;
    flag3 = 0;
    lcd.setCursor(0, 3);
    lcd.print(" AoQ Level: Fair ");
    lcd.write(byte(2));
    lcd.print("  ");
    Serial.println("AQ Level Fair");
    digitalWrite(led_R, LOW);
    digitalWrite(led_G, HIGH);
    digitalWrite(led_Y, HIGH);
  }
  else if (ppm > th4 && ppm <= th5)
  {
    flag2 = 0;
    flag3 = 0;
    lcd.setCursor(0, 3);
    lcd.print(" AoQ Lvl: Moderate ");
    lcd.write(byte(4));
    lcd.print(" ");
    Serial.println("AQ Level Moderate");
    digitalWrite(led_R, LOW);
    digitalWrite(led_G, LOW);
    digitalWrite(led_Y, HIGH);
  }
  else if (ppm > th5 && ppm <= th6)
  {
    flag2 = 0;
    lcd.setCursor(0, 3);
    lcd.print("  AoQ Level Poor ");
    Serial.println("AoQ Level Poor");
    lcd.write(byte(4));
    lcd.print("  ");
    lcd.setCursor(0, 3);
    digitalWrite(led_R, HIGH);
    digitalWrite(led_G, LOW);
    digitalWrite(led_Y, HIGH);
    if (flag3 == 0)
    {
      flag3 = 1;
      warningBuzz(2, 250);
    }
  }
  else if (ppm > th6)
  {
    lcd.setCursor(0, 3);
    lcd.print(" AoQ Lvl Hazardous ");
    lcd.write(byte(0));
    Serial.println("AQ Level DANGER");
    lcd.setCursor(0, 1);
    digitalWrite(led_R, HIGH);
    digitalWrite(led_G, LOW);
    digitalWrite(led_Y, LOW);
    if (flag2 == 0)
    {
      flag2 = 1;
      warningBuzz(3, 300);
    }
  }
  else
  {
    lcd.setCursor(0, 3);
    lcd.print("     AoQ ERROR ");
    lcd.write(byte(4));
    lcd.print("    ");
  }
}
//=====================================================
// --- Networking and Connect ---
//=====================================================
void esp_booting()
{
  if (flag1 == 0)
  {
    flag1 = 1;
    Serial.println("🚀 ESP32 Server Starting...");
    lcd.setCursor(0, 0);
    lcd.print("       ESP-32");
    lcd.setCursor(0, 1);
    lcd.print("         ^^");
    delay(500);
    lcd.setCursor(0, 2);
    lcd.print("  Wifi Connecting!");
    delay(1000);
    WiFi.mode(WIFI_STA);                        // Ensure station mode
    WiFi.config(local_IP, gateway, subnet);     // Apply static IP
    WiFi.begin(ssid, password);                 // 🔹 Now connect to Wi-Fi
    lcd.setCursor(0, 3);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      if (y < 20)
      {
        lcd.setCursor(y, 3);
        lcd.print("=");
        y++;
      }
      else
      {
        y = 0;
        lcd.setCursor(0, 3);
        lcd.print("                    ");
      }

    }
    Serial.println("\n✅ Connected to Wifi!");
    Serial.print("🔹 Server IP: ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Wifi Connected.!");
    delay(500);
    lcd.setCursor(0, 2);
    lcd.print("     Server IP:");
    Serial.println(WiFi.localIP());  // This should now be `192.168.**.** `
    lcd.setCursor(3, 3);
    lcd.print(WiFi.localIP());
    lcd.setCursor(11, 3);
    lcd.print("***.**");
    delay(4000);
    lcd.clear();
    server.begin();
  }
  else if (flag1 == 1)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Wifi-Disconnected!");
    lcd.setCursor(0, 1);
    delay(500);
    lcd.print("   Reconnecting..");
    WiFi.mode(WIFI_STA);                        // Ensure station mode
    WiFi.config(local_IP, gateway, subnet);     // Apply static IP
    WiFi.begin(ssid, password);                 // 🔹 Now connect to Wi-Fi
    lcd.setCursor(0, 3);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      if (y < 20)
      {
        lcd.setCursor(y, 3);
        lcd.print("=");
        y++;
      }
      else
      {
        y = 0;
        lcd.setCursor(0, 3);
        lcd.print("                    ");
      }
    }
    Serial.println("\n✅ ReConnected to Wifi!");
    Serial.print("🔹 Server IP: ");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Wifi Re-Connected.!");
    delay(500);
    lcd.setCursor(0, 2);
    lcd.print("     Server IP:");
    Serial.println(WiFi.localIP());  // This should now be `192.168.**.** `
    lcd.setCursor(3, 3);
    lcd.print(WiFi.localIP());
    lcd.setCursor(11, 3);
    lcd.print("***.**");
    delay(4000);
    lcd.clear();
    server.begin();
  }
}
//=====================================================
// --- CLients [ ESP1 and ESP3 ] ---
//=====================================================
void Sub_Client()
{
  WiFiClient client = server.available();// Receive Data from ESP-3
  if (client)
  {
    Serial.println("ESP-3 got connected to ESP-2..!");
    unsigned long start = millis();  // ✅ Declare the variable
    while ((client.available() < sizeof(mq5Val) + sizeof(mq5Alrm)) && (millis() - start < 1000))
    {
      delay(10); // wait a bit for data to arrive
    }
    if (client.available() >= sizeof(mq5Val) + sizeof(mq5Alrm))
    {
      client.read((uint8_t*)&mq5Val, sizeof(mq5Val));
      client.read((uint8_t*)&mq5Alrm, sizeof(mq5Alrm));
      Serial.printf("📥 Received: %d | Alarm: %s\n", mq5Val, mq5Alrm ? "true" : "false");
    }
    else
    {
      Serial.println("⚠️ Incomplete data received from ESP-3.");
    }
    client.stop();
  }
}
void Main_Client()    //Send Data to ESP-1 to Display those Data's
{
  WiFiClient client2;
  if (client2.connect(esp1_ip, esp1_port))
  {
    t1 = static_cast<int>(t);                             //convert Temp to int
    client2.write((uint8_t*)&t1, sizeof(t1));
    client2.write((uint8_t*)&h, sizeof(h));
    client2.write((uint8_t*)&ppm, sizeof(ppm));
    client2.write((uint8_t*)&mq5Alrm, sizeof(mq5Alrm));
    Serial.printf("📤 Sent: T=%d, H=%d, AoQ=%d, Alarm=%s\n", t1, h, ppm, mq5Alrm ? "true" : "false");
    client2.stop();
    delay(500);
  }
}
//=====================================================
// --- Warning Buzz ---
//=====================================================
void warningBuzz(int times, int duration)
{
  unsigned long currentTime = millis();

  // Check if we are in cooldown period
  if (currentTime - cooldownStart < TEN_MINUTES && cooldownStart != 0)
  {
    unsigned long remaining = (TEN_MINUTES - (currentTime - cooldownStart)) / 1000;
    Serial.printf("⏳ Buzzer cooldown active: %lu sec left\n", remaining);
    return; // Don't allow buzzing during cooldown
  }

  // Check if 10 minutes passed since the first buzz (window reset)
  if (buzzCount > 0 && (currentTime - firstBuzzTime >= TEN_MINUTES))
  {
    buzzCount = 0;
    firstBuzzTime = 0;
    Serial.println("🔁 10-minute window expired, counter reset");
  }

  // If no buzzes yet in this window, mark the first buzz time
  if (buzzCount == 0)
  {
    firstBuzzTime = currentTime;
  }

  // Check if we still can buzz (max 3 in window)
  if (buzzCount < 3)
  {
    // Run buzzer pattern
    for (int i = 0; i < times; i++)
    {
      digitalWrite(Buzz, LOW);   // buzzer ON
      digitalWrite(led_R, HIGH);
      delay(duration);
      digitalWrite(Buzz, HIGH);  // buzzer OFF
      digitalWrite(led_R, LOW);
      delay(duration);
    }

    buzzCount++;
    lastBuzzTime = currentTime;
    Serial.printf("⚠️ Buzzer triggered (%d/3 in current window)\n", buzzCount);

    // If we hit 3 buzzes in this 10-min window → start cooldown
    if (buzzCount >= 3)
    {
      cooldownStart = currentTime;
      Serial.println("🚫 Buzzer limit reached. Cooldown started for 10 minutes.");
    }
  }
  else
  {
    Serial.println("⚠️ Buzzer limit already reached — waiting for cooldown.");
  }
}
//=====================================================
// --- The End ---
//=====================================================
