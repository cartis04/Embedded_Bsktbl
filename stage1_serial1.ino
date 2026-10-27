// Stage 1: Button -> Serial message, Serial command -> LED
//
// Wiring:
//   Button: one leg to GPIO 4, other leg to GND (uses internal pull-up)
//   LED:    GPIO 2 is the onboard LED on most ESP32 dev boards.
//           For an external LED: GPIO -> 220 ohm resistor -> LED anode, cathode -> GND

const int BUTTON_PIN = 4;
const int LED_PIN    = 2;

bool ledState = false;
bool lastButton = HIGH;            // pull-up: HIGH = not pressed
unsigned long lastChange = 0;
const unsigned long DEBOUNCE_MS = 30;

// "Client side" function: anything that wants to control the LED calls this
void setLed(bool on) {
  ledState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  Serial.println(on ? "LED ON" : "LED OFF");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Ready. Press the button, or type 't' (toggle), '1' (on), '0' (off) and hit Enter.");
}

void loop() {
  // INPUT: physical button -> message
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButton && millis() - lastChange > DEBOUNCE_MS) {
    lastChange = millis();
    lastButton = reading;
    if (reading == LOW) {           // LOW = pressed
      Serial.println("Button pressed!");
    }
  }

  // OUTPUT: serial monitor command -> LED
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') setLed(!ledState);
    else if (c == '1') setLed(true);
    else if (c == '0') setLed(false);
  }
}
