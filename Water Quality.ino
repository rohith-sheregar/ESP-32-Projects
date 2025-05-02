#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <WebServer.h>
#include <ArduinoOTA.h>


// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_I2C_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensors
#define SOIL_SENSOR_PIN 34
#define TEMP_SENSOR_PIN 32
#define DHTTYPE DHT11
DHT dht(TEMP_SENSOR_PIN, DHTTYPE);

// WiFi Credentials
const char* ssid = "RohithHotspot";
const char* password = "12345678";

// Web Server
WebServer server(80);

// Emoji Bitmaps
const unsigned char smileyBitmap[] PROGMEM = {
  0b00111100,0b00000000,
  0b01000010,0b00000000,
  0b10100101,0b00000000,
  0b10000001,0b00000000,
  0b10100101,0b00000000,
  0b10011001,0b00000000,
  0b01000010,0b00000000,
  0b00111100,0b00000000
};

const unsigned char neutralBitmap[] PROGMEM = {
  0b00111100,0b00000000,
  0b01000010,0b00000000,
  0b10000001,0b00000000,
  0b10011001,0b00000000,
  0b10011001,0b00000000,
  0b10011001,0b00000000,
  0b01000010,0b00000000,
  0b00111100,0b00000000
};

const unsigned char frownBitmap[] PROGMEM = {
  0b00111100,0b00000000,
  0b01000010,0b00000000,
  0b10000001,0b00000000,
  0b10100101,0b00000000,
  0b10011001,0b00000000,
  0b10100101,0b00000000,
  0b01000010,0b00000000,
  0b00111100,0b00000000
};

// Global Variables
float tds = 0;
float temperature = 0;
const char* quality = "Unknown";
String emojiData = "neutral";



// Functions
float mapRawToPPM(int raw) {
  // Mapping based on known values
  if (raw >= 3800) return 0; // Air detected -> TDS: 0 PPM
  else if (raw >= 2500) return map(raw, 2500, 3500, 0, 50); // Distilled water -> TDS: 0-50 PPM
  else if (raw >= 1800 && raw < 2500) return map(raw, 1800, 2500, 50, 150); // Low Drinking water TDS
  else if (raw >= 1000 && raw < 1800) return map(raw, 1000, 1800, 150, 300); // Moderate drinking water TDS
  else if (raw >= 500 && raw < 1000) return map(raw, 500, 1000, 300, 500); // Medium saltwater TDS
  else if (raw >= 300 && raw < 500) return map(raw, 300, 500, 500, 1000); // High saltwater -> TDS: 500-1000 PPM
  else return 0; // For very low values like air or water contamination
}

float getStableRawValue() {
  const int sampleCount = 10;
  long total = 0;
  for (int i = 0; i < sampleCount; i++) {
    total += analogRead(SOIL_SENSOR_PIN);
    delay(50);
  }
  return total / sampleCount;
}

void handleData() {
  String j = "{";
  j += "\"tds\":"     + String(tds,1) + ",";
  j += "\"temp\":"    + String(temperature,1) + ",";
  j += "\"quality\":\""+ String(quality) + "\",";
  j += "\"emoji\":\"" + emojiData + "\"";
  j += "}";
  server.send(200, "application/json", j);
}


void connectToWiFi() {
  WiFi.begin(ssid, password);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Connecting WiFi...");
  display.display();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  display.clearDisplay();
  IPAddress ip = WiFi.localIP();
  display.setCursor(0, 0);
  display.println("WiFi Connected");
  display.setCursor(0, 10);
  display.print("IP: ");
  display.println(ip);
  display.display();
  Serial.println("");
  Serial.print("ESP IP address: ");
  Serial.println(ip);
}

void startOTA() {
  ArduinoOTA.begin();
}


void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(SOIL_SENSOR_PIN, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println("OLED init failed");
    while (1);
  }

  display.clearDisplay();
  display.display();

 

  connectToWiFi();
  startOTA();

  // JSON data endpoint for chart.js
  server.on("/data", handleData);
  server.begin();
}

void loop() {
ArduinoOTA.handle();
  server.handleClient();

  float rawValue = getStableRawValue();
  tds = mapRawToPPM(rawValue);
  temperature = dht.readTemperature();
  if (isnan(temperature)) temperature = 35.0;  // Default if error

  const unsigned char* emojiBitmap;

  if (rawValue >= 3800) {
    quality = "Air";
    emojiBitmap = neutralBitmap;
    emojiData = "😐";
  }
  else if (tds <= 50) {
    quality = "Excellent";
    emojiBitmap = smileyBitmap;
    emojiData = "😊";
  }
  else if (tds <= 150) {
    quality = "Good";
    emojiBitmap = smileyBitmap;
    emojiData = "😊";
  }
  else if (tds <= 300) {
    quality = "Moder";
    emojiBitmap = neutralBitmap;
    emojiData = "😐";
  }
  else if (tds <= 500) {
    quality = "Poor";
    emojiBitmap = frownBitmap;
    emojiData = "☹️";
  }
  else if (tds > 500) {
    quality = "Very Poor";
    emojiBitmap = frownBitmap;
    emojiData = "☹️";
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Water Quality");
  display.setCursor(0, 15);
  display.print("TDS: ");
  display.print(tds, 1);
  display.println(" ppm");
  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temperature, 1);
  display.println(" C");
  display.setCursor(0, 45);
  display.print("Quality: ");
  display.print(quality);
  display.drawBitmap(100, 45, emojiBitmap, 10, 8, SSD1306_WHITE);
  display.display();

  Serial.print("Raw: ");
  Serial.print(rawValue);
  Serial.print(" | TDS: ");
  Serial.print(tds);
  Serial.print(" ppm | Temp: ");
  Serial.print(temperature);
  Serial.print(" °C | Q: ");
  Serial.println(quality);

  delay(2000);
}
