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

//===============================================
// --- Libraries Used ---
//===============================================
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <SPI.h>
#include <time.h>
#include <WebServer.h>
#include <ArduinoJson.h>

//===============================================
// --- Wi-Fi and Server Constants ---
//===============================================
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

//===============================================
// --- Matrix Pins ---
//===============================================
#define MAX_DEVICES 8
#define DATA_PIN   23
#define CLK_PIN    18
#define CS_PIN_TOP    5
#define CS_PIN_BOTTOM 15
const int LDR_PIN = 32;
MD_Parola m2(MD_MAX72XX::FC16_HW, CS_PIN_TOP, MAX_DEVICES);    // Bottom row
MD_Parola m1(MD_MAX72XX::FC16_HW, CS_PIN_BOTTOM, MAX_DEVICES); // Top row

//===============================================
// --- Global State Variables ---
//===============================================
int temp = 0, hum = 0, aoq = 0, mq5Alrm = 0;
String weather_rp = "Loading weather...";
unsigned long previousMillis = 0;
const long interval = 2000;
unsigned long staticStartTime = 0;
const unsigned long staticTime = 1 * 60 * 1000;// (minutes * Seconds * milliseconds) to show the static text before switching back to scrolling
int scrollCount = 0, sc = 5; //scrollcount for the times the top matrix displays the weather report and stuff, sc for max no of scrolls.
bool showingStatic = false;
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 5 * 60 * 1000; ////minutes(the main changer) * Seconds * milliseconds to update weather report
unsigned long lastDataTime = 0;
const unsigned long timeoutMs = 5000;
bool weatherReady = false;
static bool gasDisplayed = false;  // Track if "GAS Leaking" was already displayed
//===============================================
// --- Setup ---
//===============================================
void setup()
{
  Serial.begin(115200);
  SPI.begin(CLK_PIN, -1, DATA_PIN);
  m1.begin(); m2.begin();
  m1.setIntensity(1);
  m2.setIntensity(1);
  m1.displayClear();
  m2.displayClear();
  wifi_connect();
  syncTime();           //syncronise time with the Esp module.
  internettest();       //Test the Internet is available or not.
  setupWebUI();
  weather_rp = weather_report();
  Serial.println("===================================");
  Serial.println("Weather Report : " + weather_rp);
  Serial.println("===================================");
  espserver.begin();
}
//===============================================
// --- Loop ---
//===============================================
void loop()
{
  if (WiFi.status() != WL_CONNECTED)
    wifi_connect();
  client_connect();
  if (mq5Alrm == 0)
  {
    gasDisplayed = false;
    int ldrValue = analogRead(LDR_PIN);
    int brightness1 = map(ldrValue, 0, 4095, 15, 1);
    m1.setIntensity(brightness1);
    m2.setIntensity(brightness1);
    unsigned long currentMillis = millis();

    // === WEATHER UPDATE (every 4 minutes) ===
    if (currentMillis - lastWeatherCheck >= weatherInterval)
    {
      weather_rp = weather_report();  // Fetch new weather
      lastWeatherCheck = currentMillis;
      weatherReady = true;
      Serial.println("🔁 Weather updated:");
      Serial.println(weather_rp);
      // Start new scroll cycle
      scrollCount = 0;
      showingStatic = false;
      m1.displayClear();
      m1.displayText(weather_rp.c_str(), PA_CENTER, 100, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      m1.displayReset();
    }

    // === TOP MATRIX m1 ===
    if (!weatherReady) {
      // Show default until weather loads
      m1.displayClear();
      m1.displayText("Temp Hum AoQ", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      m1.displayReset();
      weatherReady = true;  // Show it only once
    }
    if (weatherReady)
    {
      if (!showingStatic)
      {
        if (m1.displayAnimate())
        {
          scrollCount++;
          if (scrollCount >= sc)
          { // after 5 scrolls
            showingStatic = true;
            staticStartTime = currentMillis;
            m1.displayClear();
            m1.displayText("Temp Hum AoQ", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
            m1.displayReset();
          }
          else
          {
            m1.displayReset();  // Continue scroll
          }
        }
      }
      else
      {
        if (currentMillis - staticStartTime >= staticTime)
        {
          showingStatic = false;
          scrollCount = 0;
          m1.displayClear();
          m1.displayText(weather_rp.c_str(), PA_CENTER, 100, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
          m1.displayReset();
        }
      }
    }

    // === BOTTOM MATRIX m2 (always show sensor data) ===
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis;
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "%d\xB0""C %d%% %d", temp, hum, aoq);
      m2.displayClear();
      m2.displayText(buffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      m2.displayReset();
    }

    // === Watchdog: Restart if no sensor data for 5 minutes ===
    if (currentMillis - lastDataTime > 300000)
    {
      Serial.println("🛑 No sensor data for 5 minutes! Restarting ESP...");
      ESP.restart();
    }
  }
  else
  {
    while (!gasDisplayed)
    {
      m1.setIntensity(15);
      m2.setIntensity(15);
      m1.displayClear();
      m1.displayText("GAS Leaking", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      m1.displayReset();
      m2.displayClear();
      m2.displayText("!..Alert..!", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      m2.displayReset();
      gasDisplayed = true;  // Message is now shown
      break;
    }
  }
  // === Animate Displays ===
  m1.displayAnimate();  // Always animate m1
  m2.displayAnimate();  // In case needed by your library
  server.handleClient();  // Web interface
}
//===============================================
// --- Time Sync and Internet Check ---
//===============================================
void syncTime()
{
  configTime(0, 0, "time.google.com");
  Serial.print("⏳ Waiting for NTP time sync");
  time_t now = time(nullptr);
  int retries = 0;
  while (now < 1000000000 && retries < 20)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retries++;
  }
  if (now < 1000000000)
  {
    Serial.println("\n❌ Failed to sync time.");
  }
  else
  {
    Serial.println("\n✅ Time synced successfully.");
    // Format and print the synced time
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);  // Use gmtime_r or localtime_r for thread safety
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
    Serial.print("📅 Current Time: ");
    Serial.println(timeString);
  }
}
void internettest()
{
  Serial.println("🌐 Testing internet connection...");
  WiFiClient testClient;
  if (testClient.connect("example.com", 80)) {
    Serial.println("✅ Internet is working!");
    testClient.stop();
  } else {
    Serial.println("❌ No internet access!");
  }
}

//===============================================
// --- Networking Functions ---
//===============================================
void wifi_connect()
{
  WiFi.mode(WIFI_STA);                        // Ensure station mode
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wifi!");
  Serial.println("\nConnected to Wifi as ESP3: " + WiFi.localIP().toString());
  Serial.print("🔹 Server IP: ");
  Serial.println(WiFi.localIP());  // This should now be `192.168.**.** `
  delay(500);
}
void client_connect()
{
  WiFiClient client = espserver.available();
  if (client)
  {
    Serial.println("Client connected");

    while (client.connected())
    {
      if (client.available() >= sizeof(int) * 4)
      {
        int32_t t, h, a, lpg;
        client.read((uint8_t*)&t, sizeof(t));
        client.read((uint8_t*)&h, sizeof(h));
        client.read((uint8_t*)&a, sizeof(a));
        client.read((uint8_t*)&mq5Alrm, sizeof(mq5Alrm));
        // === Garbage data filter ===
        bool valid = true;
        if (t < -40 || t > 100)
        {
          t = 0;
          valid = false;
        }
        if (h < 0 || h > 100)
        {
          h = 0;
          valid = false;
        }
        if (a < 0 || a > 2000)
        {
          a = 0;
          valid = false;
        }
        if (valid)
        {
          temp = t;
          hum = h;
          aoq = a;
          lastDataTime = millis();  // ✅ Only update watchdog on good data
          Serial.printf("✅ ESP1 received: Temp=%d, Hum=%d, AOQ=%d, Alarm=%s\n", temp, hum, aoq, mq5Alrm ? "true" : "false");
        }
        else
        {
          temp = 0;
          hum = 0;
          aoq = 0;
          Serial.println("⚠️ Invalid or garbage data received. Client disconnected.");
        }
      }
    }
    client.stop();
  }
}
//===============================================
// --- Weather ---
//===============================================
float cityTemp = 0;
float cityFeelsLike = 0;
float cityMax = 0;
float cityMin = 0;
String cityTimeOfDay = "Unknown";
String cityCondition = "Unknown";
float cityHumidity = 0;
float cityWind = 0;
String cityWindDir = "Unknown";
float cityPressure = 0;
float cityVisibility = 0;
float cityUV = 0;
String weather_report()
{
  const char* city = "26.7003,88.4335";
  const char* apiKey = "HgQWzfx8gcpiEXYr04XqKwFfPCSVEIda";
  String url = "https://api.tomorrow.io/v4/weather/realtime?location=" + String(city) + "&apikey=" + String(apiKey);

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  HTTPClient https;
  https.begin(secureClient, url);
  int httpCode = https.GET();

  if (httpCode > 0)
  {
    String payload = https.getString();
    https.end();

    const size_t capacity = 4096;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print("❌ JSON Parse Error: ");
      Serial.println(error.c_str());
      return "Error parsing weather data";
    }
    JsonObject values = doc["data"]["values"];

    // Assign values to globals
    cityTemp = values["temperature"];
    cityFeelsLike = values["temperatureApparent"];
    cityHumidity = values["humidity"];
    cityWind = values["windSpeed"];
    cityWindDir = getCompassDirection(values["windDirection"]);
    cityPressure = values["pressureSeaLevel"];
    cityVisibility = values["visibility"];
    cityUV = values["uvIndex"];
    int code = values["weatherCode"];
    cityCondition = getWeatherDescription(code);
    String timeUTC = doc["data"]["time"];     // e.g., "2025-05-26T08:07:00Z"

    int hourUTC = timeUTC.substring(11, 13).toInt(); // Convert UTC time string to hour (simplified for categorization)
    int minuteUTC = timeUTC.substring(14, 16).toInt();
    int hourIST = hourUTC + 5;           // Add 5 hours and 30 minutes to convert to IST
    int minuteIST = minuteUTC + 30;
    if (minuteIST >= 60)                // Adjust overflow
    {
      minuteIST -= 60;
      hourIST += 1;
    }
    if (hourIST >= 24)
    {
      hourIST -= 24;
    }
    // Time Display for day or night in IST
    if (hourIST >= 4 && hourIST < 6) cityTimeOfDay = "Early Morning";
    else if (hourIST >= 6 && hourIST < 9) cityTimeOfDay = "Morning";
    else if (hourIST >= 9 && hourIST < 12) cityTimeOfDay = "Late Morning";
    else if (hourIST >= 12 && hourIST < 14) cityTimeOfDay = "Early Afternoon";
    else if (hourIST >= 14 && hourIST < 17) cityTimeOfDay = "Afternoon";
    else if (hourIST >= 17 && hourIST < 19) cityTimeOfDay = "Evening";
    else if (hourIST >= 19 && hourIST < 21) cityTimeOfDay = "Late Evening";
    else if (hourIST >= 21 || hourIST < 1) cityTimeOfDay = "Night";
    else if (hourIST >= 1 && hourIST < 4) cityTimeOfDay = "Midnight";
    Serial.println(code);
    return "City Temp: " + String(cityTemp, 1) + "C | Feels: " + String(cityFeelsLike, 1) + "C | " + cityCondition;
  }
  else
  {
    Serial.printf("❌ HTTPS Error: %d\n", httpCode);
    https.end();
    return "Error fetching weather";
  }
}
//===============================================
// --- Compass Code ---
//===============================================
String getCompassDirection(float degrees)
{
  const char* directions[] =
  {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
  };
  int index = (int)((degrees + 11.25) / 22.5) % 16;
  return String(directions[index]);
}
//===============================================
// --- Weather Code ---
//===============================================
String getWeatherDescription(int code)
{
  switch (code)
  {
    case 1000: return "Clear";
    case 1100: return "Mostly Clear";
    case 1101: return "Partly Cloudy";
    case 1102: return "Mostly Cloudy";
    case 1001: return "Cloudy";
    case 2000: return "Fog";
    case 2100: return "Light Fog";
    case 4000: return "Drizzle";
    case 4001: return "Rain";
    case 4200: return "Light Rain";
    case 4201: return "Heavy Rain";
    case 5000: return "Snow";
    case 5001: return "Flurries";
    case 5100: return "Light Snow";
    case 5101: return "Heavy Snow";
    case 6000: return "Freezing Drizzle";
    case 6001: return "Freezing Rain";
    case 6200: return "Light Freezing Rain";
    case 6201: return "Heavy Freezing Rain";
    case 7000: return "Ice Pellets";
    case 7101: return "Heavy Ice Pellets";
    case 7102: return "Light Ice Pellets";
    case 8000: return "Thunderstorm";
    default: return "Unknown";
  }
}
//===============================================
// --- Web UI ---
//===============================================
void setupWebUI()
{
  server.on("/", []()
  {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8" />
      <meta name="viewport" content="width=device-width, initial-scale=1" />
      <meta http-equiv="refresh" content="10" />
      <title>Room Monitor</title>
      <style>
        body {
          background: #121212;
          color: #e0e0e0;
          font-family: 'Consolas', 'Courier New', monospace;
          margin: 0;
          padding: 20px;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: flex-start;
          min-height: 100vh;
          user-select: none;
        }
        h1 {
          color: #00ffd1;
          margin-bottom: 20px;
          text-shadow: 0 0 8px #00ffd1;
        }
        .container {
          background: #1f1f1f;
          border-radius: 12px;
          padding: 20px 40px;
          box-shadow: 0 0 15px #00ffd1aa;
          width: 320px;
          max-width: 90vw;
        }
        .item {
          font-size: 1.3em;
          margin: 12px 0;
          padding-bottom: 6px;
          border-bottom: 1px solid #00ffd1aa;
          display: flex;
          justify-content: space-between;
        }
        .label {
          font-weight: 700;
          color: #00ffd1;
        }
        .value {
          font-weight: 400;
          color: #b0fefe;
          text-align: right;
        }
        .weather-section {
          margin-top: 20px;
          border-top: 1px solid #00ffd1aa;
          padding-top: 15px;
        }
        .weather-item {
          font-size: 1.1em;
          margin: 8px 0;
          display: flex;
          justify-content: space-between;
        }
        .weather-label {
          font-weight: bold;
          color: #00ffd1;
        }
        .weather-value {
          color: #b0fefe;
          text-align: right;
        }
        footer {
          margin-top: 40px;
          font-size: 0.75em;
          color: #444;
          text-align: center;
          font-family: Arial, sans-serif;
        }
        .animated-gradient {
          background: linear-gradient(270deg, #00ffd1, #00e0ff, #0099ff, #00ffd1);
          background-size: 800% 800%;
          -webkit-background-clip: text;
          -webkit-text-fill-color: transparent;
          animation: gradientBG 12s ease infinite;
        }
        @keyframes gradientBG {
          0%{background-position:0% 50%}
          50%{background-position:100% 50%}
          100%{background-position:0% 50%}
        }
      </style>
      <script>
        let countdown = 10;
        function updateTimer() {
          if(countdown <= 0) return;
          document.getElementById('refreshTimer').textContent = countdown + "s";
          countdown--;
        }
        setInterval(updateTimer, 1000);
        window.onload = updateTimer;

        function formatAMPM(date) {
          let hours = date.getHours();
          const minutes = date.getMinutes().toString().padStart(2, '0');
          const ampm = hours >= 12 ? 'PM' : 'AM';
          hours = hours % 12;
          hours = hours ? hours : 12;
          return hours + ':' + minutes + ' ' + ampm;
        }

        function updateTimes() {
          const nowUTC = new Date();
          const utcStr = formatAMPM(nowUTC) + " IST";

          const ist = new Date(nowUTC.getTime() + (5.5 * 60 * 60 * 1000));
          const istStr = formatAMPM(ist) + " UTC";

          document.getElementById('utcTime').textContent = utcStr;
          document.getElementById('istTime').textContent = istStr;
        }

        setInterval(updateTimes, 1000);
        window.onload = function() {
          updateTimer();
          updateTimes();
        };
      </script>
    </head>
    <body>
      <h1 class="animated-gradient">Room Monitor</h1>
      <div class="container">
    )rawliteral";

    // Basic Sensor Data
    html += "<div class='item'><span class='label'>Temperature:</span><span class='value'>" + String(temp) + " °C</span></div>";
    html += "<div class='item'><span class='label'>Humidity:</span><span class='value'>" + String(hum) + " %</span></div>";
    html += "<div class='item'><span class='label'>Air Quality:</span><span class='value'>" + String(aoq) + "</span></div>";

    // Full Weather Data
    html += "<div class='weather-section'>";
    html += "<div class='weather-item'><span class='weather-label'>City Temperature:</span><span class='weather-value'>" + String(cityTemp, 1) + " °C</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Feels Like:</span><span class='weather-value'>" + String(cityFeelsLike, 1) + " °C</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Condition:</span><span class='weather-value'>" + cityCondition + "</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Humidity:</span><span class='weather-value'>" + String(cityHumidity, 1) + " %</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Wind Speed:</span><span class='weather-value'>" + String(cityWind, 1) + " m/s</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Wind Direction:</span><span class='weather-value'>" + cityWindDir + "</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Pressure:</span><span class='weather-value'>" + String(cityPressure, 2) + " hPa</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>UV Index:</span><span class='weather-value'>" + String(cityUV, 1) + "</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Visibility:</span><span class='weather-value'>" + String(cityVisibility, 1) + " km</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Time of Day (IST):</span><span class='weather-value'>" + cityTimeOfDay + "</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>IST Time:</span><span class='weather-value' id='utcTime'>--:--</span></div>";
    html += "<div class='weather-item'><span class='weather-label'>Standard Time:</span><span class='weather-value' id='istTime'>--:--</span></div>";
    html += "</div>";

    // Close HTML
    html += R"rawliteral(
      </div>
      <footer>Page refreshes in <span id="refreshTimer">10s</span></footer>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html; charset=UTF-8", html);
  });

  server.begin();
  Serial.println("🌐 Web server started at http://" + WiFi.localIP().toString());
}
//=====================================================
// --- String Converter for Matrix Display ---
//=====================================================
String fixUTF8(String input) //Not used anymore .... I used this for printing Degree Symbol in the Marix then left the IDea.,,
{
  input.replace("°", "\xB0"); // replaces UTF-8 degree with extended ASCII degree
  return input;
}
//===============================================
// --- The End ---
//===============================================
