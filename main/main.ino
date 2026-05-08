#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// U8g2 Setup for SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const char* ssid = "doğuş";
const char* password = "probis12";
const String serverUrl = "http://192.168.36.178:8000";

Servo myServo;
int servoPin = 18;
int buzzerPin = 19;

enum State { IDLE, MENU, ACTION };
State currentState = MENU;
String lastMessage = "Sistem Hazir";

TaskHandle_t animationTask;
volatile bool isProcessing = false;

// Function Prototypes
void showIdleAnimation();
void showMenu();
void performAction();
void checkForStateChange();
void processCommand(String cmd);
void playTone(int freq, int duration);
void waveServo();

void drawJumpingDots(void * parameter) {
  int dotState = 0;
  while (isProcessing) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x13_tr);
    u8g2.drawUTF8(25, 25, "Isleniyor...");
    
    for (int i = 0; i < 3; i++) {
      int y = 45;
      if (i == dotState) y = 40; 
      u8g2.drawFilledEllipse(45 + (i * 15), y, 3, 3);
    }
    
    u8g2.sendBuffer();
    dotState = (dotState + 1) % 3;
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);
  
  u8g2.begin();
  u8g2.enableUTF8Print(); 
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawUTF8(0, 15, "WiFi'ye Baglaniliyor...");
  u8g2.sendBuffer();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Baglandi");
  
  u8g2.clearBuffer();
  u8g2.setCursor(0, 15);
  u8g2.print("WiFi Baglantisi Tamam!");
  u8g2.sendBuffer();
  delay(1000);
  
  dht.begin();
  myServo.attach(servoPin);
  pinMode(buzzerPin, OUTPUT);
  
  playTone(1000, 200);
  waveServo();
}

void loop() {
  checkForStateChange();
  
  switch(currentState) {
    case IDLE:
      showIdleAnimation();
      break;
    case MENU:
      showMenu();
      break;
    case ACTION:
      performAction();
      break;
  }
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
}

void showIdleAnimation() {
  u8g2.clearBuffer();
  u8g2.drawFilledEllipse(40, 32, 10, 10); // Left Eye
  u8g2.drawFilledEllipse(88, 32, 10, 10); // Right Eye
  u8g2.sendBuffer();
  delay(2000);
  
  // Blink
  u8g2.clearBuffer();
  u8g2.drawBox(30, 30, 20, 4);
  u8g2.drawBox(78, 30, 20, 4);
  u8g2.sendBuffer();
  delay(150);
}

void showMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(serverUrl + "/events");
    int httpResponseCode = http.GET();
    
    if(httpResponseCode > 0){
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        const char* currentTime = doc["currentTime"] | "00:00";
        int totalEvents = doc["count"] | 0;
        JsonArray events = doc["events"].as<JsonArray>();

        u8g2.setFont(u8g2_font_7x13_tr);

        // Top Section
        u8g2.setCursor(0, 12);
        u8g2.print("["); u8g2.print(lastMessage); u8g2.print("]");
        
        u8g2.setCursor(0, 28);
        float t = dht.readTemperature();
        if (isnan(t)) {
          u8g2.drawUTF8(0, 28, "Hata");
        } else {
          u8g2.print(t, 1);
          u8g2.drawUTF8(u8g2.getCursorX(), 28, "\xC2\xB0" "C");
        }
        
        int clockWidth = u8g2.getUTF8Width(currentTime);
        u8g2.setCursor(128 - clockWidth - 2, 28);
        u8g2.print(currentTime);
        
        u8g2.drawLine(0, 32, 128, 32);
        
        // Bottom Section
        u8g2.drawUTF8(0, 46, "Bugun ");
        u8g2.setCursor(u8g2.getUTF8Width("Bugun ") + 2, 46);
        u8g2.print(totalEvents);
        u8g2.drawUTF8(u8g2.getCursorX() + 4, 46, "Toplanti");
        
        int y = 58;
        int count = 0;
        for (JsonVariant v : events) {
          if (count >= 1) break; 
          const char* summary = v["summary"] | "Basliksiz";
          const char* timeStr = v["timeString"] | "";
          
          u8g2.setCursor(0, y);
          u8g2.print("> ");
          u8g2.print(timeStr);
          u8g2.print(" ");
          u8g2.print(summary);
          count++;
        }
      }
    }
    http.end();
  }
  u8g2.sendBuffer();
}

void processCommand(String cmd) {
  cmd.trim();
  String lowerCmd = cmd;
  lowerCmd.toLowerCase();

  if (lowerCmd == "idle" || lowerCmd == "bekle") {
    currentState = IDLE;
    return;
  } else if (lowerCmd == "menu" || lowerCmd == "menü") {
    currentState = MENU;
    return;
  }
  
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(serverUrl + "/command");
    http.addHeader("Content-Type", "application/json");
    String httpRequestData = "{\"voiceCommand\":\"" + cmd + "\"}";
    
    isProcessing = true;
    xTaskCreatePinnedToCore(drawJumpingDots, "AnimTask", 4096, NULL, 1, &animationTask, 0);
    
    int httpResponseCode = http.POST(httpRequestData);
    isProcessing = false;
    delay(300);
    
    if(httpResponseCode > 0){
      String response = http.getString();
      Serial.println(response);
    }
    http.end();
  }
  currentState = ACTION;
}

void checkForStateChange() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 2000) return;
  lastCheck = millis();
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl + "/state");
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(256);
      deserializeJson(doc, payload);
      const char* serverState = doc["state"] | "";
      if (strlen(serverState) > 0) {
        State newState = (strcmp(serverState, "IDLE") == 0) ? IDLE : MENU;
        if (newState != currentState) currentState = newState;
      }
    }
    http.end();
  }
}

void performAction() {
  playTone(1500, 100);
  waveServo();
  currentState = MENU;
}

void playTone(int freq, int duration) {
  tone(buzzerPin, freq, duration);
}

void waveServo() {
  myServo.write(0);
  delay(500);
  myServo.write(90);
  delay(500);
}
