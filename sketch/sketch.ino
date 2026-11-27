#include <mutex>
#include <LittleFS.h>

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include "AudioGeneratorMP3.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioOutputI2SNoDAC.h"

const int POT_PIN_MAX_VALUE = 4095; // ESP32 ADC is 12-bit (0-4095)

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

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceLittleFS *file = nullptr;
AudioOutputI2SNoDAC *out = nullptr;

void setupSound() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  out = new AudioOutputI2SNoDAC(25);
  out->SetOutputModeMono(true);
  out->SetGain(0.0); // Keep silent until a sound actually starts
}

void stopSound() {
  if (mp3) {
    if (mp3->isRunning()) mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }
  if (out) {
    out->SetGain(0.0); // Prevents idle DAC hiss/static
  }
}

void playSound() {
  // 1. Feed the active MP3 decoder if a track is currently playing
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      stopSound();
      Serial.println("Finished sound");
    }
    return;
  }

  // 2. Check if BLE wrote a new sound trigger
  int requestedSound = SILENT;
  {
    std::scoped_lock<std::mutex> lock(mtx);
    requestedSound = sound;
    sound = SILENT; // Consume the trigger so it doesn't restart every loop()
  }

  if (requestedSound == SILENT) return;

  const char *path = nullptr;
  if (requestedSound == FAILURE) path = "/failure.mp3";
  else if (requestedSound == SUCCESS) path = "/success.mp3";
  else if (requestedSound == NEUTRAL) path = "/neutral.mp3";
  else return;

  stopSound(); // Clean up any previous file/decoder before opening a new one

  Serial.printf("Playing %s\n", path);
  file = new AudioFileSourceLittleFS(path);
  mp3 = new AudioGeneratorMP3();
  out->SetGain(0.8);
  mp3->begin(file, out);
}
#pragma endregion

#pragma region BLUETOOTH CONTROL
const char *bleName = "ESP32_Bluetooth";
volatile unsigned long lastMessageTime = 0;

#define SERVICE_UUID "01a219a4-df08-71be-b29e-15f1d0017ddf"
#define CHARACTERISTIC_UUID_RX "01a219a5-0c99-73f9-9fca-973d5e5b121d"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Connected");
  }
  void onDisconnect(BLEServer *pServer) {
    Serial.println("Disconnected");
    delay(500);
    pServer->startAdvertising();
  }
};

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

void setupBLE() {
  BLEDevice::init(bleName);
  BLEServer *pServer = BLEDevice::createServer();
  if (pServer == nullptr) {
    Serial.println("Error creating BLE server");
    return;
  }
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("Error creating BLE service");
    return;
  }
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection...");
}
#pragma endregion

#pragma region main

void setup() {
  Serial.begin(115200);
  ledcAttach(LED_RED, LED_FREQ, RESOLUTION);
  ledcAttach(LED_GREEN, LED_FREQ, RESOLUTION);
  ledcAttach(LED_BLUE, LED_FREQ, RESOLUTION);
  setupBLE();
  setupSound();
}

void loop() {
  setColor(analogRead(BRIGHTNESS_POT_PIN));
  playSound();

  std::scoped_lock<std::mutex> lock(mtx);
  if (millis() - lastMessageTime > 10000) {
    red = green = blue = 0;
  }
}

#pragma endregion