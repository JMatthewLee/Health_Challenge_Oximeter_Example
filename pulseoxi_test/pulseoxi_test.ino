#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobot_MAX30102.h"

// Screen Variables
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

DFRobot_MAX30102 particleSensor;

// Time-based averaging (15 seconds)
const int MAX_READINGS = 300; // 15 seconds at 50ms intervals = 300 readings
struct Reading {
  int heartRate;
  int spo2;
  unsigned long timestamp;
};

Reading readingStack[MAX_READINGS];
int stackTop = -1; // Stack pointer

// Calibration tracking
bool isCalibrating = false;
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_TIME = 15000; // 15 seconds

void setup() {
  Serial.begin(9600);
  
  // Initialize reading stack
  for (int i = 0; i < MAX_READINGS; i++) {
    readingStack[i].heartRate = 0;
    readingStack[i].spo2 = 0;
    readingStack[i].timestamp = 0;
  }
  
  // Initialize OLED Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Show initialization message
  display.setCursor(0, 0);
  display.print("MAX30102 Init...");
  display.display();
  
  Serial.println("MAX30102 Pulse Oximeter Test");
  Serial.println("Initializing...");
  
  // Initialize I2C
  Wire.begin();
  
  // Try initializing sensor with explicit parameters
  if (particleSensor.begin(&Wire, 0x57)) {
    Serial.println("MAX30102 initialized successfully!");
    display.setCursor(0, 16);
    display.print("Success!");
    display.display();
  } else {
    Serial.println("MAX30102 initialization failed!");
    Serial.println("But I2C device detected at 0x57...");
    Serial.println("Attempting to continue anyway...");
    display.setCursor(0, 16);
    display.print("Continuing...");
    display.display();
  }
  
  delay(1000);
  
  // Configure sensor
  Serial.println("Configuring sensor...");
  particleSensor.sensorConfiguration(
    /*ledBrightness=*/50,
    /*sampleAverage=*/SAMPLEAVG_4,
    /*ledMode=*/MODE_MULTILED,
    /*sampleRate=*/SAMPLERATE_400,
    /*pulseWidth=*/PULSEWIDTH_411,
    /*adcRange=*/ADCRANGE_4096
  );
  
  Serial.println("Configuration complete!");
  Serial.println("\nPlace your finger on the sensor with steady pressure.");
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Place finger");
  display.setCursor(0, 16);
  display.print("on sensor...");
  display.display();
  delay(2000);
}

void pushReading(int hr, int spo2) {
  unsigned long currentTime = millis();
  
  // Push new reading onto stack
  stackTop++;
  if (stackTop >= MAX_READINGS) {
    stackTop = MAX_READINGS - 1;
    // Shift all readings down (remove oldest)
    for (int i = 0; i < MAX_READINGS - 1; i++) {
      readingStack[i] = readingStack[i + 1];
    }
  }
  
  readingStack[stackTop].heartRate = hr;
  readingStack[stackTop].spo2 = spo2;
  readingStack[stackTop].timestamp = currentTime;
}

void removeOldReadings() {
  unsigned long currentTime = millis();
  unsigned long fifteenSecondsAgo = currentTime - 15000;
  
  // Remove readings older than 15 seconds from bottom of stack
  int newBottom = 0;
  for (int i = 0; i <= stackTop; i++) {
    if (readingStack[i].timestamp >= fifteenSecondsAgo) {
      newBottom = i;
      break;
    }
  }
  
  // Shift stack if needed
  if (newBottom > 0) {
    int shift = newBottom;
    for (int i = 0; i <= stackTop - shift; i++) {
      readingStack[i] = readingStack[i + shift];
    }
    stackTop -= shift;
    if (stackTop < -1) stackTop = -1;
  }
}

int calculateAverageHeartRate() {
  if (stackTop < 0) return 0;
  
  int sum = 0;
  int count = 0;
  
  for (int i = 0; i <= stackTop; i++) {
    if (readingStack[i].heartRate > 0 && readingStack[i].heartRate < 200) {
      sum += readingStack[i].heartRate;
      count++;
    }
  }
  
  return (count > 0) ? (sum / count) : 0;
}

int calculateAverageSPO2() {
  if (stackTop < 0) return 0;
  
  int sum = 0;
  int count = 0;
  
  for (int i = 0; i <= stackTop; i++) {
    if (readingStack[i].spo2 > 0 && readingStack[i].spo2 <= 100) {
      sum += readingStack[i].spo2;
      count++;
    }
  }
  
  return (count > 0) ? (sum / count) : 0;
}

void drawBeatingHeart(int x, int y) {
  // Simple constant pulsing animation (not synced to heart rate)
  unsigned long currentTime = millis();
  
  // Create a sine wave effect for smooth pulsing
  // Complete pulse cycle every 1000ms (1 second)
  float cycle = (currentTime % 1000) / 1000.0; // 0.0 to 1.0
  
  // Create pulse pattern: quick beat, then rest
  int scale;
  if (cycle < 0.1) {
    // Quick expansion
    scale = 100 + (int)(cycle * 10 * 30); // 100 to 130
  } else if (cycle < 0.2) {
    // Quick contraction
    scale = 130 - (int)((cycle - 0.1) * 10 * 30); // 130 to 100
  } else if (cycle < 0.3) {
    // Small second beat
    scale = 100 + (int)((cycle - 0.2) * 10 * 15); // 100 to 115
  } else if (cycle < 0.4) {
    // Return to normal
    scale = 115 - (int)((cycle - 0.3) * 10 * 15); // 115 to 100
  } else {
    // Rest phase
    scale = 100;
  }
  
  // Draw heart shape (scaled)
  int s = scale;
  // Left curve
  display.fillCircle(x - (3 * s / 100), y - (2 * s / 100), (3 * s / 100), SSD1306_WHITE);
  // Right curve
  display.fillCircle(x + (3 * s / 100), y - (2 * s / 100), (3 * s / 100), SSD1306_WHITE);
  // Bottom triangle
  display.fillTriangle(
    x - (6 * s / 100), y - (1 * s / 100),
    x + (6 * s / 100), y - (1 * s / 100),
    x, y + (6 * s / 100),
    SSD1306_WHITE
  );
}

void loop() {
  int32_t SPO2 = 0;
  int8_t SPO2Valid = 0;
  int32_t heartRate = 0;
  int8_t heartRateValid = 0;
  
  // Get heart rate and SpO2 values
  particleSensor.heartrateAndOxygenSaturation(&SPO2, &SPO2Valid, &heartRate, &heartRateValid);
  
  // Get raw values for finger detection
  uint32_t redValue = particleSensor.getRed();
  uint32_t irValue = particleSensor.getIR();
  
  // Check if finger is detected
  bool fingerDetected = (redValue > 10000 && irValue > 10000);
  
  // Handle calibration state
  if (fingerDetected && !isCalibrating && stackTop < 0) {
    // Just placed finger, start calibration
    isCalibrating = true;
    calibrationStartTime = millis();
  } else if (!fingerDetected) {
    // Finger removed, reset calibration
    isCalibrating = false;
    calibrationStartTime = 0;
  } else if (isCalibrating && (millis() - calibrationStartTime >= CALIBRATION_TIME)) {
    // Calibration complete
    isCalibrating = false;
  }
  
  // Calculate remaining calibration time
  int calibrationSecondsLeft = 0;
  if (isCalibrating) {
    unsigned long elapsed = millis() - calibrationStartTime;
    calibrationSecondsLeft = 15 - (elapsed / 1000);
    if (calibrationSecondsLeft < 0) calibrationSecondsLeft = 0;
  }
  
  // Push valid readings to stack
  if (fingerDetected && heartRateValid == 1 && SPO2Valid == 1) {
    if (heartRate > 0 && heartRate < 200 && SPO2 > 0 && SPO2 <= 100) {
      pushReading(heartRate, SPO2);
    }
  }
  
  // Remove readings older than 15 seconds
  removeOldReadings();
  
  // Calculate averages from last 15 seconds
  int avgHeartRate = calculateAverageHeartRate();
  int avgSPO2 = calculateAverageSPO2();
  
  // Clear display
  display.clearDisplay();
  
  if (!fingerDetected) {
    // Display "Waiting for finger" message
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print("WAITING FOR");
    display.setCursor(28, 35);
    display.print("FINGER...");
    
    // Animated dots
    int dotCount = (millis() / 500) % 4;
    display.setCursor(88, 35);
    for (int i = 0; i < dotCount; i++) {
      display.print(".");
    }
  } else if (isCalibrating) {
    // Display calibration screen
    display.setTextSize(1);
    display.setCursor(15, 10);
    display.print("CALIBRATING...");
    
    // Draw progress bar
    int barWidth = 100;
    int barHeight = 10;
    int barX = 14;
    int barY = 30;
    
    unsigned long elapsed = millis() - calibrationStartTime;
    int progress = (elapsed * barWidth) / CALIBRATION_TIME;
    if (progress > barWidth) progress = barWidth;
    
    // Draw progress bar outline
    display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
    // Fill progress
    if (progress > 0) {
      display.fillRect(barX + 1, barY + 1, progress - 2, barHeight - 2, SSD1306_WHITE);
    }
    
    // Display countdown
    display.setTextSize(2);
    display.setCursor(50, 45);
    display.print(calibrationSecondsLeft);
    display.setTextSize(1);
    display.print("s");
    
  } else {
    // Title
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("PULSE OXIMETER");
    
    // Draw separator line
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Draw animated beating heart
    drawBeatingHeart(110, 28);
    
    // Average Heart Rate - Large text
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("HR:");
    if (avgHeartRate > 0) {
      display.print(avgHeartRate);
      display.setTextSize(1);
      display.setCursor(70, 24);
      display.print("BPM");
    } else {
      display.print("--");
      display.setTextSize(1);
      display.setCursor(70, 24);
      display.print("BPM");
    }
    
    // SpO2 - Large text
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.print("O2:");
    if (avgSPO2 > 0) {
      display.print(avgSPO2);
      display.setTextSize(1);
      display.setCursor(85, 44);
      display.print("%");
    } else {
      display.print("--");
    }
    
    // Status indicator with reading count
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("n:");
    display.print(stackTop + 1);
    
    // Filled circle when valid data
    if (heartRateValid == 1 || SPO2Valid == 1) {
      display.fillCircle(120, 58, 3, SSD1306_WHITE);
    } else {
      display.drawCircle(120, 58, 3, SSD1306_WHITE);
    }
  }
  
  // Display everything
  display.display();
  
  // Serial output for debugging
  Serial.println("========================");
  Serial.print("Finger Detected: ");
  Serial.println(fingerDetected ? "YES" : "NO");
  Serial.print("Instant Heart Rate: ");
  if (heartRateValid == 1) {
    Serial.print(heartRate);
    Serial.println(" BPM");
  } else {
    Serial.println("-- BPM (Invalid)");
  }
  
  Serial.print("Average Heart Rate (15s): ");
  Serial.print(avgHeartRate);
  Serial.println(" BPM");
  
  Serial.print("Instant SpO2: ");
  if (SPO2Valid == 1) {
    Serial.print(SPO2);
    Serial.println(" %");
  } else {
    Serial.println("-- % (Invalid)");
  }
  
  Serial.print("Average SpO2 (15s): ");
  Serial.print(avgSPO2);
  Serial.println(" %");
  
  Serial.print("Stack Size: ");
  Serial.println(stackTop + 1);
  
  Serial.print("Red LED: ");
  Serial.println(redValue);
  Serial.print("IR LED: ");
  Serial.println(irValue);
  Serial.println("========================\n");
  
  delay(50); // Faster refresh for smooth animation
}
