#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo myServo;
int servoPin = 18;
int buzzerPin = 19;

enum State { IDLE, MENU, ACTION };
State currentState = IDLE;

void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.display();
  
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
  // Fetch from API logic here
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
  // Send to API logic here
  currentState = ACTION;
}

void performAction() {
  playTone(1500, 100);
  waveServo();
  currentState = IDLE;
}
