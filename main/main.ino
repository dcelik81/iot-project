#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "doğuş";
const char* password = "probis12";
const String serverUrl = "http://192.168.36.178:8000";

Servo myServo;
int servoPin = 18;
int buzzerPin = 19;

enum State { IDLE, MENU, ACTION };
State currentState = MENU;

void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi Connected!");
  display.display();
  delay(1000);
  
  myServo.attach(servoPin);
  pinMode(buzzerPin, OUTPUT);
  
  // Wave on startup
  playTone(1000, 200);
  waveServo();
}

void loop() {
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
  
  // Simulate voice command via Serial for testing
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    processCommand(command);
  }
}

void showIdleAnimation() {
  display.clearDisplay();
  // Draw "Blinking Eyes"
  display.fillCircle(40, 32, 10, WHITE); // Left Eye
  display.fillCircle(88, 32, 10, WHITE); // Right Eye
  display.display();
  delay(2000);
  
  // Blink
  display.clearDisplay();
  display.fillRect(30, 30, 20, 4, WHITE);
  display.fillRect(78, 30, 20, 4, WHITE);
  display.display();
  delay(150);
}

void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Calendar Events:");
  
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(serverUrl + "/events");
    int httpResponseCode = http.GET();
    
    if(httpResponseCode > 0){
      String payload = http.getString();
      
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        display.println("Parse Error");
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      } else {
        JsonArray events = doc["events"].as<JsonArray>();
        int count = 0;
        for (JsonVariant v : events) {
          if (count >= 5) break; // OLED can fit ~5-6 lines of text
          const char* summary = v["summary"] | "No Title";
          const char* timeStr = v["timeString"] | "";
          
          display.print(summary);
          display.print(" ");
          display.println(timeStr);
          count++;
        }
        if (count == 0) {
          display.println("No events found.");
        }
      }
    } else {
      display.println("Error fetching");
    }
    http.end();
  } else {
    display.println("WiFi Disconnected");
  }
  
  display.display();
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

void processCommand(String cmd) {
  Serial.print("Processing: ");
  Serial.println(cmd);
  
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    http.begin(serverUrl + "/command");
    http.addHeader("Content-Type", "application/json");
    
    // Simple JSON string construction
    String httpRequestData = "{\"voiceCommand\":\"" + cmd + "\"}";
    int httpResponseCode = http.POST(httpRequestData);
    
    if(httpResponseCode > 0){
      String response = http.getString();
      Serial.print("API Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
  
  currentState = ACTION;
}

void performAction() {
  playTone(1500, 100);
  waveServo();
  currentState = IDLE;
}
