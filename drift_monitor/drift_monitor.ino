#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/* WiFi credentials */
#define WIFI_SSID "JD"
#define WIFI_PASSWORD "Jd@2007."

/* Firebase credentials */
#define API_KEY "AIzaSyBAOqKeQG9sNPY3pQDh1g-SpgAwbN8iYG4"
#define DATABASE_URL "https://esp32-drift-monitor-default-rtdb.asia-southeast1.firebasedatabase.app"

/* DHT sensor */
#define DHTPIN 4
#ifndef DHTTYPE
#define DHTTYPE DHT11
#endif

/* I2C LCD (16x2) */
#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

unsigned long lastSensorReadMs = 0;
const unsigned long sensorIntervalMs = 5000;
const float tempAlertThresholdC = 30.0;
const float humLowAlertThreshold = 20.0;
const float humHighAlertThreshold = 80.0;

void printBanner() {
  Serial.println();
  Serial.println("=====================================");
  Serial.println("   ESP32 DRIFT MONITOR v3.0");
  Serial.println("=====================================");
}

void updateLcd(const String &line1, const String &line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, LCD_COLUMNS));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, LCD_COLUMNS));
}

void connectWiFi() {
  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    updateLcd("WiFi connected", WiFi.localIP().toString());
    return;
  }

  Serial.println("[WiFi] Connection failed.");
  updateLcd("WiFi failed", "Retrying...");
}

bool pushMetric(const char *path, float value) {
  const int maxAttempts = 2;
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(DATABASE_URL) + path + ".json";
    if (!http.begin(client, url)) {
      Serial.println("[UPLOAD] HTTP begin failed");
      continue;
    }

    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(String(value, 2));
    String response = http.getString();
    http.end();

    if (httpCode >= 200 && httpCode < 300) {
      return true;
    }

    Serial.print("[UPLOAD] Failed for ");
    Serial.print(path);
    Serial.print(" attempt ");
    Serial.print(attempt);
    Serial.print('/');
    Serial.print(maxAttempts);
    Serial.print(" | HTTP ");
    Serial.print(httpCode);
    Serial.print(" | ");
    Serial.println(response);

    if (httpCode == -1 && WiFi.status() != WL_CONNECTED) {
      Serial.println("[UPLOAD] WiFi lost during upload.");
      connectWiFi();
    }

    delay(200);
  }

  return false;
}

bool pushHistoryEntry(float temperature, float humidity, bool tempAlert, bool humAlert) {
  const int maxAttempts = 2;
  for (int attempt = 1; attempt <= maxAttempts; attempt++) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(DATABASE_URL) + "/sensor/history.json";
    if (!http.begin(client, url)) {
      Serial.println("[HISTORY] HTTP begin failed");
      continue;
    }

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 2) + ",";
    payload += "\"humidity\":" + String(humidity, 2) + ",";
    payload += "\"tempAlert\":" + String(tempAlert ? "true" : "false") + ",";
    payload += "\"humAlert\":" + String(humAlert ? "true" : "false") + ",";
    payload += "\"ts\":{\".sv\":\"timestamp\"}";
    payload += "}";

    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);
    String response = http.getString();
    http.end();

    if (httpCode >= 200 && httpCode < 300) {
      return true;
    }

    Serial.print("[HISTORY] Push failed attempt ");
    Serial.print(attempt);
    Serial.print('/');
    Serial.print(maxAttempts);
    Serial.print(" | HTTP ");
    Serial.print(httpCode);
    Serial.print(" | ");
    Serial.println(response);

    if (httpCode == -1 && WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }

    delay(200);
  }

  return false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  printBanner();

  Wire.begin();
  lcd.init();
  lcd.backlight();
  updateLcd("Drift Monitor", "Booting...");

  dht.begin();
  delay(1000);
  Serial.print("[DHT] Sensor initialized on pin ");
  Serial.println(DHTPIN);

  float testTemp = dht.readTemperature();
  float testHum = dht.readHumidity();
  if (isnan(testTemp) || isnan(testHum)) {
    Serial.println("[DHT] Initial read failed. Check wiring and sensor type.");
    updateLcd("DHT read failed", "Check wiring");
  } else {
    Serial.print("[DHT] Initial read OK -> Temp: ");
    Serial.print(testTemp);
    Serial.print("C, Humidity: ");
    Serial.print(testHum);
    Serial.println("%");
    updateLcd("DHT ready", "Sensor OK");
  }

  connectWiFi();
  Serial.println("[Firebase] Using RTDB REST upload mode");
  updateLcd("Firebase REST OK", "Monitoring...");

  Serial.println("=====================================");
  Serial.println("   SETUP COMPLETE - Starting loop");
  Serial.println("=====================================");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected! Reconnecting...");
    updateLcd("WiFi reconnect", "Please wait...");
    connectWiFi();
    return;
  }

  if (millis() - lastSensorReadMs < sensorIntervalMs) {
    return;
  }
  lastSensorReadMs = millis();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT] ERROR: Failed to read sensor!");
    Serial.println("[DHT] Check: VCC->3.3V, DATA->GPIO4 with 10K pullup, GND->GND");
    updateLcd("DHT read error", "Check sensor");
    return;
  }

  Serial.print("[DATA] Temp: ");
  Serial.print(temperature);
  Serial.print("C | Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  bool tempOk = pushMetric("/sensor/temperature", temperature);
  bool humOk = pushMetric("/sensor/humidity", humidity);

  if (tempOk) {
    Serial.println("[UPLOAD] Temperature -> OK");
  }
  if (humOk) {
    Serial.println("[UPLOAD] Humidity -> OK");
  }

  String line1 = "T:" + String(temperature, 1) + "C H:" + String(humidity, 0) + "%";
  bool tempAlert = temperature > tempAlertThresholdC;
  bool humAlert = humidity < humLowAlertThreshold || humidity > humHighAlertThreshold;
  bool historyOk = pushHistoryEntry(temperature, humidity, tempAlert, humAlert);

  String line2 = "Sync: OK";
  if (!tempOk || !humOk || !historyOk) {
    line2 = "Sync: Error";
  }
  if (tempAlert && humAlert) {
    line2 = "ALERT:TEMP+HUM";
  } else if (tempAlert) {
    line2 = "ALERT:TEMP HIGH";
  } else if (humAlert) {
    line2 = "ALERT:HUM DRIFT";
  }

  if (tempAlert || humAlert) {
    Serial.print("[ALERT] ");
    if (tempAlert) {
      Serial.print("Temp high ");
    }
    if (humAlert) {
      Serial.print("Humidity drift");
    }
    Serial.println();
  }

  if (historyOk) {
    Serial.println("[HISTORY] Entry -> OK");
  }

  updateLcd(line1, line2);

  Serial.println("---");
}
