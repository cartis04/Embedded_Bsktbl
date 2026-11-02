
const int sensorPin = 18;

const unsigned long detectionDelay = 50;

int previousState = HIGH;
int currentState = HIGH;

unsigned long lastChangeTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(sensorPin, INPUT);

  Serial.println("Basketball Break Beam Test");
  Serial.println("--------------------------");

  currentState = digitalRead(sensorPin);
  previousState = currentState;

  if (currentState == LOW) {
    Serial.println("BEAM BLOCKED");
  } else {
    Serial.println("BEAM CLEAR");
  }
}

void loop() {

  int reading = digitalRead(sensorPin);

  if (reading != previousState) {
    lastChangeTime = millis();
    previousState = reading;
  }

  if ((millis() - lastChangeTime) > detectionDelay) {

    if (reading != currentState) {

      currentState = reading;

      if (currentState == LOW) {
        Serial.println("BALL DETECTED!");
      } else {
        Serial.println("BEAM CLEAR");
      }
    }
  }
}
