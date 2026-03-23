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
#include <LiquidCrystal_PCF8574.h>
#include <DHT.h>
//=====================================================
// --- Wi-Fi and Server Constants ---
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
WiFiServer server(esp3_port);
//=====================================================
// --- Matrix Pins ---
//=====================================================
#define MQ5_PIN 34                 //Mq5 Sensor
#define R_Led 15                   //Led Blinker
#define Buzz 18
#define DHTPIN 27        // Or any free GPIO pin
#define DHTTYPE DHT11    // Use DHT11 instead of DHT22
int mq5Value = 69;
int mq5Alrm = 0;
int y = 0, f = 0, temp = 3, hum = 3;
float  temp2 = 3.00;
LiquidCrystal_PCF8574 lcd(0x23);
DHT dht(DHTPIN, DHTTYPE);
//=====================================================
// --- Setup ---
//=====================================================
void setup()
{
  Serial.begin(115200);
  dht.begin();
  lcd.begin(16, 2);
  lcd.setBacklight(255);  // Turn on backlight
  pinMode(R_Led, OUTPUT);
  pinMode(Buzz, OUTPUT);
  int mq5Value = analogRead(MQ5_PIN);
  digitalWrite(R_Led, HIGH);
  digitalWrite(Buzz, HIGH);
  wifi_connect();
  lcd.setCursor(0, 0);
  lcd.print("Sensor Warming..");
  unsigned long totalSeconds = 5 * 60;  // 5 minutes in seconds
  for (unsigned long i = totalSeconds; i > 0; i--)
  {
    lcd.setCursor(2, 1);
    unsigned long minutes = i / 60;
    unsigned long seconds = i % 60;
    lcd.print("Wait: ");
    if (minutes < 10) lcd.print("0");
    lcd.print(minutes);
    lcd.print(":");
    if (seconds < 10) lcd.print("0");
    lcd.print(seconds);
    lcd.print("    ");  // Clear leftover characters
    mq5Value = analogRead(MQ5_PIN);
    Serial.println("MQ5 Raw: " + String(mq5Value));
    delay(1000);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Sensor Ready!");
  delay(2000);  // Optional short message before starting actual loop
  lcd.clear();
}
//=====================================================
// --- Loop ---
//=====================================================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
    wifi_connect();
  mq5Value = analogRead(MQ5_PIN);
  digitalWrite(R_Led, HIGH);
  lcd.setCursor(0, 0);
  lcd.print(" LPG : ");
  lcd.print(mq5Value);
  lcd.setCursor(11, 0);
  lcd.print("ppm");
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print((temp));
  lcd.setCursor(4, 1);
  lcd.print(" H:");
  lcd.print((hum) - 3);
  showstat(mq5Value);
  Serial.printf("MQ5 Raw: %d | Alarm: %s\n", mq5Value, mq5Alrm ? "true" : "false");
  SendData();
  delay(2000);
  lcd.clear();
}
//=====================================================
// --- Networking Functions ---
//=====================================================
void wifi_connect()
{

  WiFi.mode(WIFI_STA);                        // Ensure station mode
  WiFi.config(local_IP, gateway, subnet);     // Apply static IP
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 0);
  lcd.print("Wifi-Connecting!");
  delay(1000);
  lcd.setCursor(0, 1);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (y < 16)
    {
      lcd.setCursor(y, 1);
      lcd.print("=");
      y++;
    }
    else
    {
      y = 0;
      lcd.setCursor(0, 1);
      lcd.print("                ");
    }
  }
  Serial.println("\n✅ Connected to Wifi!");
  Serial.println("\nConnected to Wifi as ESP3: " + WiFi.localIP().toString());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   Server IP: ");
  Serial.print("🔹 Server IP: ");
  Serial.println(WiFi.localIP());  // This should now be `192.168.**.** `
  lcd.setCursor(1, 1);
  lcd.print(WiFi.localIP());
  lcd.setCursor(9, 1);
  lcd.print("***.**");
  delay(3000);
  lcd.clear();
}

void SendData()
{
  WiFiClient client;
  if (client.connect(esp2_ip, esp2_port))
  {
    client.write((uint8_t*)&mq5Value, sizeof(mq5Value));
    client.write((uint8_t*)&mq5Alrm, sizeof(mq5Alrm));
    Serial.printf("📤 Sent: %d | Alarm: %s to ESP-2(Server)\n", mq5Value, mq5Alrm ? "true" : "false");
    Serial.println();
    client.stop();
  }
  else
  {
    Serial.println("Connection to ESP2 failed");
  }
}
//=====================================================
// --- Display ---
//=====================================================
void showstat(int val)
{
  enviro();

  int safeThreshold, cautionThreshold;
  calculateThresholds(temp, hum, safeThreshold, cautionThreshold);

  // 🧠 Smooth sensor (kills spikes)
  static int filteredVal = 0;
  filteredVal = (filteredVal * 0.8) + (val * 0.2);

  if (filteredVal > cautionThreshold)
  {
    // 🚨 FULL ALARM (only 3200–4000+)
    mq5Alrm = 1;
    lcd.setCursor(0, 1);
    lcd.print(" GAS LEAKING!!! ");
    digitalWrite(R_Led, HIGH);
    warningBuzz(5, 150);
  }
  else if (filteredVal > safeThreshold)
  {
    // ⚠️ WARNING ZONE (2500–3200 approx)
    mq5Alrm = 0;
    lcd.setCursor(9, 1);
    lcd.print(" ALERT ");
    digitalWrite(R_Led, HIGH);

    if (f == 0)
    {
      f = 1;
      warningBuzz(2, 200);
    }
  }
  else
  {
    // ✅ SAFE
    mq5Alrm = 0;
    lcd.setCursor(9, 1);
    lcd.print(" SAFE  ");
    digitalWrite(R_Led, LOW);
    f = 0;
  }

  Serial.printf("🌡 %d°C | 💧 %d%% | Safe=%d | Alert=%d | MQ=%d (Filtered=%d)\n",
                temp, hum, safeThreshold, cautionThreshold, val, filteredVal);
}
void calculateThresholds(int temp, int hum, int &safeThreshold, int &cautionThreshold)
{
  // Environment
  const int baseTemp = 20, maxTemp = 38;
  const int baseHum  = 35, maxHum  = 95;

  // 🔥 MUCH HIGHER thresholds (less sensitive)
  const int safeBase = 2400, safeMax = 3600;
  const int alertBase = 3200, alertMax = 4095;

  temp = constrain(temp, baseTemp, maxTemp);
  hum  = constrain(hum, baseHum, maxHum);

  float tempRatio = float(temp - baseTemp) / (maxTemp - baseTemp);
  float humRatio  = float(hum - baseHum) / (maxHum - baseHum);

  // Reduce humidity effect (important for winter)
  float combinedRatio = (tempRatio * 0.7) + (humRatio * 0.3);

  // Smooth curve
  combinedRatio = pow(combinedRatio, 1.6);

  safeThreshold    = safeBase   + (int)((safeMax   - safeBase)   * combinedRatio);
  cautionThreshold = alertBase  + (int)((alertMax  - alertBase)  * combinedRatio);

  // ❄️ Winter compensation
  if (temp < 24) {
    int boost = map(temp, 20, 24, 600, 200);
    safeThreshold    += boost;
    cautionThreshold += boost;
  }

  // Keep proper gap
  if (cautionThreshold - safeThreshold < 400)
    cautionThreshold = safeThreshold + 400;
}

void enviro()
{
  hum = dht.readHumidity();
  temp = dht.readTemperature() - 3;
  
  if (isnan(hum) || isnan(temp))
  {
    Serial.println("❌ Failed to read from DHT11 sensor!");
  } else {
    //Serial.printf("Fake : 🌡 Temp: %d°C | 💧 Humidity: %d%%\n", temp, hum);
  }
}
//=====================================================
// --- Buzzer Warning ---
//=====================================================
void warningBuzz(int times, int duration )
{
  for (int i = 0; i < times; i++)
  {
    digitalWrite(Buzz, LOW);  // Turn buzzer ON
    delay(duration);                     // Wait 0.5 seconds
    digitalWrite(Buzz, HIGH);   // Turn buzzer OFF
    delay(duration);
  }
}
//=====================================================
// --- The End ---
//=====================================================w
