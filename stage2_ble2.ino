#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Stage 2 (BLE): Phone <-> ESP32 over Bluetooth Low Energy
//
// - Physical button press -> BLE "notify" message to the phone
// - Phone writes to the LED characteristic -> LED changes
//
// Wiring (same as before):
//   Button: GPIO 4 to GND (internal pull-up)
//   LED:    GPIO 2 (onboard LED on most boards)
//
// Phone: use the free "nRF Connect" app (see instructions in chat).
// Works with Arduino-ESP32 core 2.x and 3.x. Uses the built-in BLE library.



#define SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BUTTON_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // ESP32 -> phone (notify)
#define LED_CHAR_UUID    "d1e2f3a4-0000-4a5b-9c8d-123456789abc"  // phone -> ESP32 (write)

const int BUTTON_PIN = 4;
const int LED_PIN    = 2;

BLEServer* server = nullptr;
BLECharacteristic* buttonChar = nullptr;
bool deviceConnected = false;

bool ledState = false;
bool lastButton = HIGH;
unsigned long lastChange = 0;
const unsigned long DEBOUNCE_MS = 30;

void setLed(bool on) {
  ledState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  Serial.println(on ? "LED ON" : "LED OFF");
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    deviceConnected = true;
    Serial.println("Phone connected");
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
    Serial.println("Phone disconnected, advertising again");
    BLEDevice::startAdvertising();   // so you can reconnect
  }
};

// Called when the phone writes to the LED characteristic
class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    auto value = c->getValue();      // std::string on core 2.x, String on core 3.x
    if (value.length() == 0) return;
    char cmd = value[0];
    if (cmd == '1' || cmd == 1)      setLed(true);
    else if (cmd == '0' || cmd == 0) setLed(false);
    else if (cmd == 't' || cmd == 2) setLed(!ledState);
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  BLEDevice::init("ESP32-Demo");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  // ESP32 -> phone: button messages
  buttonChar = service->createCharacteristic(
      BUTTON_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
#if ESP_ARDUINO_VERSION_MAJOR < 3
  buttonChar->addDescriptor(new BLE2902());   // needed on core 2.x; automatic on 3.x
#endif
  buttonChar->setValue("Ready");

  // Phone -> ESP32: LED control
  BLECharacteristic* ledChar = service->createCharacteristic(
      LED_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  ledChar->setCallbacks(new LedCallbacks());

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("Advertising as 'ESP32-Demo'. Connect with nRF Connect.");
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButton && millis() - lastChange > DEBOUNCE_MS) {
    lastChange = millis();
    lastButton = reading;
    if (reading == LOW) {
      Serial.println("Button pressed!");
      if (deviceConnected) {
        buttonChar->setValue("Button pressed!");
        buttonChar->notify();
      }
    }
  }
}
