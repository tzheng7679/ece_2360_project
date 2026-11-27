#include <mutex>
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioOutputI2S.h"

const int POT_PIN_MAX_VALUE = 1023;

#pragma region RGB LED CONTROL

const int LED_RED = 27;
const int LED_GREEN = 14;
const int LED_BLUE = 12;
const int LED_FREQ = 5000;
const int BRIGHTNESS_POT_PIN = 32;
const int RESOLUTION = 8;

std::mutex mtx;
int red = 0;
int green = 0;
int blue = 0;

// sets color of the led; accepts red green and blue levels as integers 0-255
void setColor(int pot_reading) {
  std::scoped_lock<std::mutex> lock(mtx);
  ledcWrite(LED_RED, red * pot_reading / POT_PIN_MAX_VALUE);
  ledcWrite(LED_GREEN, green * pot_reading / POT_PIN_MAX_VALUE);
  ledcWrite(LED_BLUE, blue * pot_reading / POT_PIN_MAX_VALUE);
}

#pragma endregion

#pragma region SOUND CONTROL
int sound = 0;

const int SILENT = 0;
const int FAILURE = 1;
const int SUCCESS = 2;
const int NEUTRAL = 3;

AudioGeneratorMP3 *mp3;
AudioFileSourceLittleFS *FAILURE_FILE;
AudioFileSourceLittleFS *SUCCESS_FILE;
AudioFileSourceLittleFS *NEUTRAL_FILE;
AudioOutputI2S *out;

void setupSound() {
  FAILURE_FILE = new AudioFileSourceLittleFS("/failure.mp3");
  SUCCESS_FILE = new AudioFileSourceLittleFS("/success.mp3");
  NEUTRAL_FILE = new AudioFileSourceLittleFS("/neutral.mp3");

  out = new AudioOutputI2S(0, 1);
  out->SetOutputModeMono(true);
  mp3 = new AudioGeneratorMP3();
}

void playSound() {
  if (mp3 && mp3->isRunning()) {
    if(!mp3->loop()) mp3->stop();
    delete mp3;
    mp3 = nullptr;
  } else {
    return;
  }

  AudioFileSourceLittleFS *file;

  std::scoped_lock<std::mutex> lock(mtx);
  if (sound == 0) return;
  Serial.printf("Playing sound");
  if (sound == FAILURE) file = FAILURE_FILE;
  if (sound == SUCCESS) file = SUCCESS_FILE;
  if (sound == NEUTRAL) file = NEUTRAL_FILE;
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
}
#pragma endregion

#pragma region BLUETOOTH CONTROL
// Define the Bluetooth device name
const char *bleName = "ESP32_Bluetooth";

unsigned long lastMessageTime = 0;

// Define the UUIDs of the service and characteristics
#define SERVICE_UUID "01a219a4-df08-71be-b29e-15f1d0017ddf"
#define CHARACTERISTIC_UUID_RX "01a219a5-0c99-73f9-9fca-973d5e5b121d"

// Define the Bluetooth characteristic
BLECharacteristic *pCharacteristic;

// Define the BLE server callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  // Print the crnnection message when a client is connected
  void onConnect(BLEServer *pServer) {
    Serial.println("Connected");
  }
  // Print the disconnection message when a client is disconnected
  void onDisconnect(BLEServer *pServer) {
    Serial.println("Disconnected");
    delay(1000);
    pServer->startAdvertising();
  }
};

// Define the BLE characteristic callbacks
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t *value = pCharacteristic->getData();
    size_t len = pCharacteristic->getLength();

    if (len != 4) return;

    std::scoped_lock<std::mutex> lock(mtx);
    lastMessageTime = millis();
    red = value[0];
    green = value[1];
    blue = value[2];
    sound = value[3];
  }
};

// Initialize the Bluetooth BLE
void setupBLE() {
  BLEDevice::init(bleName);                        // Initialize the BLE device
  BLEServer *pServer = BLEDevice::createServer();  // Create the BLE server
  // Print the error message if the BLE server creation fails
  if (pServer == nullptr) {
    Serial.println("Error creating BLE server");
    return;
  }
  pServer->setCallbacks(new MyServerCallbacks());  // Set the BLE server callbacks

  // Create the BLE service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // Print the error message if the BLE service creation fails
  if (pService == nullptr) {
    Serial.println("Error creating BLE service");
    return;
  }
  // Create the BLE characteristic for receiving data
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCharacteristicCallbacks());  // Set the BLE characteristic callbacks
  pService->start();                                                 // Start the BLE service
  pServer->getAdvertising()->start();                                // Start advertising
  Serial.println("Waiting for a client connection...");              // Wait for a client connection
}

#pragma endregion

#pragma region main

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  ledcAttach(LED_RED, LED_FREQ, RESOLUTION);
  ledcAttach(LED_GREEN, LED_FREQ, RESOLUTION);
  ledcAttach(LED_BLUE, LED_FREQ, RESOLUTION);
  setupBLE();
  setupSound();
}


void loop() {
  // When the received text is not empty and the time since the last message is over 1 second
  // Send a notification and print the received text
  setColor(analogRead(BRIGHTNESS_POT_PIN));
  playSound();
  std::scoped_lock<std::mutex> lock(mtx);
  if (millis() - lastMessageTime > 1000) {
    red = green = blue = sound = 0;
  }
}

#pragma endregion