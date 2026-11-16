#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#if ESP_ARDUINO_VERSION_MAJOR < 3
#include <BLE2902.h>   // only needed on core 2.x; automatic on 3.x
#endif

// Break-beam shot detector over BLE for the Hoop Shot Clock app.
//
// - Beam broken (ball passes)  -> "BALL DETECTED" notify on SENSOR char (app counts this as a made shot)
// - Beam restored              -> "BEAM CLEAR" notify on SENSOR char (app ignores this)
// - Running shot total         -> text number on SCORE char (read/notify, for debugging in nRF Connect)
// - Phone writes '1' / '0' / 't' to LED char -> LED on / off / toggle (app flashes it on each made shot)
//
// Changes from the original:
//   * score is only notified when it changes (it was being notified on every pass through loop(),
//     which floods the BLE link and can drop the shot notifications)
//   * notifications are only sent while a phone is connected
//   * BLE2902 include added for core 2.x
//   * score is sent as readable text

#define SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SENSOR_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // ESP32 -> phone (notify)
#define LED_CHAR_UUID    "d1e2f3a4-0000-4a5b-9c8d-123456789abc"  // phone -> ESP32 (write)
#define SCORE_CHAR_UUID  "b4d42e04-5c44-49e5-bd10-e4ac9912088d"  // ESP32 -> phone (score)

const int sensorPin = 18;
const int LED_PIN   = 2;
const unsigned long detectionDelay = 50;   // beam must stay changed this long (ms) to count

int previousState = HIGH;
int currentState  = HIGH;
int score = 0;
unsigned long lastChangeTime = 0;

BLEServer* server = nullptr;
BLECharacteristic* sensorChar = nullptr;
BLECharacteristic* scoreChar  = nullptr;
bool deviceConnected = false;
bool ledState = false;

void setLed(bool on) {
  ledState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  Serial.println(on ? "LED ON" : "LED OFF");
}

void sendText(BLECharacteristic* c, const char* text, bool notifyPhone) {
  c->setValue((uint8_t*)text, strlen(text));
  if (notifyPhone && deviceConnected) c->notify();
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    deviceConnected = true;
    Serial.println("Phone connected");
  }
  void onDisconnect(BLEServer*) override {
    deviceConnected = false;
    Serial.println("Phone disconnected, advertising again");
    BLEDevice::startAdvertising();
  }
};

class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    auto value = c->getValue();   // std::string on core 2.x, String on core 3.x
    if (value.length() == 0) return;
    char cmd = value[0];
    if (cmd == '1' || cmd == 1)      setLed(true);
    else if (cmd == '0' || cmd == 0) setLed(false);
    else if (cmd == 't' || cmd == 2) setLed(!ledState);
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT);   // if readings float, try INPUT_PULLUP (depends on your receiver)
  pinMode(LED_PIN, OUTPUT);

  BLEDevice::init("ESP32-Demo");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  scoreChar = service->createCharacteristic(
      SCORE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  sensorChar = service->createCharacteristic(
      SENSOR_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
#if ESP_ARDUINO_VERSION_MAJOR < 3
  scoreChar->addDescriptor(new BLE2902());
  sensorChar->addDescriptor(new BLE2902());
#endif
  sendText(sensorChar, "Ready", false);
  sendText(scoreChar, "0", false);

  BLECharacteristic* ledChar = service->createCharacteristic(
      LED_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  ledChar->setCallbacks(new LedCallbacks());

  service->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Advertising as 'ESP32-Demo'.");

  currentState = digitalRead(sensorPin);
  previousState = currentState;
  Serial.println(currentState == LOW ? "BEAM BLOCKED" : "BEAM CLEAR");
}

void loop() {
  int reading = digitalRead(sensorPin);

  if (reading != previousState) {
    lastChangeTime = millis();
    previousState = reading;
  }

  // Beam has held a new state long enough: treat it as a real change
  if ((millis() - lastChangeTime) > detectionDelay && reading != currentState) {
    currentState = reading;

    if (currentState == LOW) {
      score++;
      Serial.print("BALL DETECTED! Score: ");
      Serial.println(score);
      char num[12];
      snprintf(num, sizeof(num), "%d", score);
      sendText(scoreChar, num, false);           // readable value only, no notification

      char msg[24];
      snprintf(msg, sizeof(msg), "BALL DETECTED,%d", score);
      sendText(sensorChar, msg, true);           // the ONE notification per shot
    } else {
      Serial.println("Beam Clear");
      sendText(sensorChar, "BEAM CLEAR", false); // value only: less BLE traffic, app ignores it anyway
    }
  }

  // Heartbeat every 2 s (only when the beam is quiet) so the app can confirm notifications reach the phone
  static unsigned long lastBeat = 0;
  if (deviceConnected && millis() - lastBeat > 2000 && millis() - lastChangeTime > 500) {
    lastBeat = millis();
    sendText(sensorChar, "ALIVE", true);
  }
}
