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
String lastMessage = "System Ready";

TaskHandle_t animationTask;
volatile bool isProcessing = false;

void drawJumpingDots(void * parameter) {
  int dotState = 0;
  while (isProcessing) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(25, 20);
    display.print("Processing");
    
    for (int i = 0; i < 3; i++) {
      int y = 40;
      if (i == dotState) y = 35; 
      display.fillCircle(45 + (i * 15), y, 3, WHITE);
    }
    
    display.display();
    dotState = (dotState + 1) % 3;
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

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
      } else {
        const char* currentTime = doc["currentTime"] | "00:00";
        int totalEvents = doc["count"] | 0;
        JsonArray events = doc["events"].as<JsonArray>();

        // Top Section
        display.setTextSize(1);
        display.setCursor(5, 2);
        display.print(lastMessage); 
        
        display.setCursor(5, 18);
        display.print("YUKI");
        
        display.setCursor(95, 18);
        display.print(currentTime);
        
        // Divider
        display.drawLine(0, 30, 128, 30, WHITE);
        
        // Bottom Section
        display.setCursor(5, 35);
        display.print(totalEvents);
        display.print(" Meetings Today");
        
        int y = 46;
        int count = 0;
        for (JsonVariant v : events) {
          if (count >= 2) break; // Only room for 2 events in this layout
          const char* summary = v["summary"] | "No Title";
          const char* timeStr = v["timeString"] | "";
          
          display.setCursor(5, y);
          display.print("> ");
          display.print(timeStr);
          display.print(" ");
          display.print(summary);
          
          y += 10;
          count++;
        }
        
        if (totalEvents == 0) {
          display.setCursor(5, 46);
          display.print("No upcoming events");
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
  cmd.trim();
  String lowerCmd = cmd;
  lowerCmd.toLowerCase();

  Serial.print("Processing: ");
  Serial.println(cmd);

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
    
    // Simple JSON string construction
    String httpRequestData = "{\"voiceCommand\":\"" + cmd + "\"}";
    
    // Start animation task
    isProcessing = true;
    xTaskCreatePinnedToCore(drawJumpingDots, "AnimTask", 4096, NULL, 1, &animationTask, 0);
    
    int httpResponseCode = http.POST(httpRequestData);
    
    // Stop animation task
    isProcessing = false;
    delay(300); // Wait for task to finish deleting itself
    
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
  currentState = MENU;
}
void checkForStateChange() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 2000) return; // Check every 2 seconds
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
        if (newState != currentState) {
          currentState = newState;
          Serial.print("Server triggered state change to: ");
          Serial.println(serverState);
        }
      }
    }
    http.end();
  }
}
