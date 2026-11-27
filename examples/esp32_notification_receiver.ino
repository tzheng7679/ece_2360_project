/*
  ESP32 Bluetooth Notification Receiver (ECE 2360 Final Project)
  
  Listens for notification signals sent from laptop over BluetoothSerial.
  Adjusts RGB LED color and buzzer/speaker tone based on notification type:
    - WARNING: Red LED + Alarm whine tone
    - SUCCESS: Green LED + Pleasant high chime
    - INFO:    Blue LED + Short beep
    - LOW:     Dim White / Yellow LED + Low chirp
  
  LED brightness is scaled by potentiometer on GPIO 32.
  Speaker volume/pitch is scaled by potentiometer on GPIO 33.
*/

#include "BluetoothSerial.h"

// Check if Bluetooth is properly enabled in ESP32 Arduino core
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

// --- Pin Definitions ---
const int PIN_LED_RED   = 25; // Red channel
const int PIN_LED_GREEN = 26; // Green channel
const int PIN_LED_BLUE  = 27; // Blue channel
const int PIN_BUZZER    = 14; // Speaker / Buzzer audio output
const int PIN_POT_BRIGHTNESS = 32; // Potentiometer 1 (ADC1_CH4)
const int PIN_POT_VOLUME     = 33; // Potentiometer 2 (ADC1_CH5)

// --- PWM Channels for LED control ---
const int PWM_CHAN_R = 0;
const int PWM_CHAN_G = 1;
const int PWM_CHAN_B = 2;
const int PWM_FREQ   = 5000;
const int PWM_RES    = 8; // 8-bit resolution (0-255)

// Duration to keep notification cue active (5 seconds as per design)
const unsigned long ACTIVE_DURATION_MS = 5000;
unsigned long activeUntil = 0;
bool isAlertActive = false;

// Current base RGB values (0-255)
int baseR = 0, baseG = 0, baseB = 0;
int activeFrequency = 0;

void setLEDs(int r, int g, int b, float scale) {
  ledcWrite(PWM_CHAN_R, (int)(r * scale));
  ledcWrite(PWM_CHAN_G, (int)(g * scale));
  ledcWrite(PWM_CHAN_B, (int)(b * scale));
}

void setup() {
  Serial.begin(115200);
  
  // Start Bluetooth Serial with device name
  SerialBT.begin("ESP32_Notification_LED");
  Serial.println("Bluetooth started. Device name: ESP32_Notification_LED");

  // Setup PWM for RGB LED
  ledcSetup(PWM_CHAN_R, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CHAN_G, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CHAN_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_LED_RED, PWM_CHAN_R);
  ledcAttachPin(PIN_LED_GREEN, PWM_CHAN_G);
  ledcAttachPin(PIN_LED_BLUE, PWM_CHAN_B);

  // Setup Buzzer pin
  pinMode(PIN_BUZZER, OUTPUT);

  // Turn off initially
  setLEDs(0, 0, 0, 0);
  noTone(PIN_BUZZER);
}

void triggerCue(String type) {
  type.trim();
  Serial.print("Triggering cue for: ");
  Serial.println(type);

  if (type.indexOf("WARNING") >= 0) {
    baseR = 255; baseG = 0; baseB = 0; // Red
    activeFrequency = 880;             // Warning alarm pitch
  } else if (type.indexOf("SUCCESS") >= 0) {
    baseR = 0; baseG = 255; baseB = 0; // Green
    activeFrequency = 1320;            // Pleasant chime pitch
  } else if (type.indexOf("LOW") >= 0) {
    baseR = 100; baseG = 80; baseB = 0; // Dim amber
    activeFrequency = 440;             // Gentle tone
  } else { // Default INFO
    baseR = 0; baseG = 120; baseB = 255; // Blue
    activeFrequency = 660;              // Standard beep
  }

  isAlertActive = true;
  activeUntil = millis() + ACTIVE_DURATION_MS;
}

void loop() {
  // 1. Check for incoming Bluetooth data
  if (SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      Serial.print("Received over Bluetooth: ");
      Serial.println(incoming);
      triggerCue(incoming);
    }
  }

  // Also allow testing via USB Serial monitor
  if (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    incoming.trim();
    if (incoming.length() > 0) {
      Serial.print("Received over USB: ");
      Serial.println(incoming);
      triggerCue(incoming);
    }
  }

  // 2. Handle active alert state and potentiometer scaling
  if (isAlertActive) {
    if (millis() < activeUntil) {
      // Read potentiometer 1 for brightness (0 - 4095)
      int potBright = analogRead(PIN_POT_BRIGHTNESS);
      float brightScale = (float)potBright / 4095.0;

      // Read potentiometer 2 for volume/intensity (0 - 4095)
      int potVol = analogRead(PIN_POT_VOLUME);
      float volScale = (float)potVol / 4095.0;

      // Update LED with scaled brightness
      setLEDs(baseR, baseG, baseB, brightScale);

      // Play sound scaled by volume potentiometer
      if (volScale > 0.05 && activeFrequency > 0) {
        tone(PIN_BUZZER, (int)(activeFrequency * (0.8 + 0.4 * volScale)));
      } else {
        noTone(PIN_BUZZER);
      }
    } else {
      // 5 seconds elapsed: turn off
      isAlertActive = false;
      setLEDs(0, 0, 0, 0);
      noTone(PIN_BUZZER);
      Serial.println("Alert finished. Turning off LED & buzzer.");
    }
  }

  delay(20);
}
