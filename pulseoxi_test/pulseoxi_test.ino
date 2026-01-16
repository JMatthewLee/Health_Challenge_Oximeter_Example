#include <Wire.h>
#include "DFRobot_MAX30102.h"
#include "rgb_lcd.h"

DFRobot_MAX30102 particleSensor;
rgb_lcd lcd;

void setup() {
  Serial.begin(9600);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0); // Green backlight
  lcd.clear();
  lcd.print("MAX30102 Init...");
  
  Serial.println("MAX30102 Pulse Oximeter Test");
  Serial.println("Initializing...");
  
  // Initialize I2C
  Wire.begin();
  
  // Try initializing sensor with explicit parameters
  if (particleSensor.begin(&Wire, 0x57)) {
    Serial.println("MAX30102 initialized successfully!");
    lcd.setCursor(0, 1);
    lcd.print("Success!");
  } else {
    Serial.println("MAX30102 initialization failed!");
    Serial.println("But I2C device detected at 0x57...");
    Serial.println("Attempting to continue anyway...");
    lcd.setCursor(0, 1);
    lcd.print("Continuing...");
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
  
  lcd.clear();
  lcd.print("Place finger...");
  delay(2000);
}

void loop() {
  int32_t SPO2 = 0;
  int8_t SPO2Valid = 0;
  int32_t heartRate = 0;
  int8_t heartRateValid = 0;
  
  // Get heart rate and SpO2 values
  particleSensor.heartrateAndOxygenSaturation(&SPO2, &SPO2Valid, &heartRate, &heartRateValid);
  
  // Get raw values for diagnostics
  uint32_t redValue = particleSensor.getRed();
  uint32_t irValue = particleSensor.getIR();
  
  // Clear LCD and display readings
  lcd.clear();
  
  // Row 1: Heart Rate
  lcd.setCursor(0, 0);
  lcd.print("HR:");
  if (heartRateValid == 1 && heartRate > 0) {
    lcd.print(heartRate);
    lcd.print("bpm");
    lcd.setRGB(0, 255, 0); // Green - good reading
  } else {
    lcd.print("--");
    lcd.setRGB(255, 255, 0); // Yellow - waiting
  }
  
  // Row 2: SpO2
  lcd.setCursor(0, 1);
  lcd.print("SpO2:");
  if (SPO2Valid == 1 && SPO2 > 0) {
    lcd.print(SPO2);
    lcd.print("%");
  } else {
    lcd.print("--");
  }
  
  // Add temperature on row 1 if space available
  if (heartRateValid == 1) {
    lcd.setCursor(11, 0);
    float tempC = particleSensor.readTemperatureC();
    lcd.print(tempC, 0);
    lcd.print("C");
  }
  
  // Serial output for debugging
  Serial.println("========================");
  Serial.print("Heart Rate: ");
  if (heartRateValid == 1) {
    Serial.print(heartRate);
    Serial.println(" BPM");
  } else {
    Serial.println("-- BPM (Invalid)");
  }
  
  Serial.print("SpO2: ");
  if (SPO2Valid == 1) {
    Serial.print(SPO2);
    Serial.println(" %");
  } else {
    Serial.println("-- % (Invalid)");
  }
  
  Serial.print("Red LED: ");
  Serial.println(redValue);
  Serial.print("IR LED: ");
  Serial.println(irValue);
  
  if (redValue == 0 && irValue == 0) {
    Serial.println("WARNING: No readings! Check finger placement.");
  } else if (redValue < 10000 || irValue < 10000) {
    Serial.println("Note: Low signal. Press finger more firmly.");
  }
  
  Serial.print("Temperature: ");
  Serial.print(particleSensor.readTemperatureC());
  Serial.println(" °C");
  Serial.println("========================\n");
  
  delay(1000);
}