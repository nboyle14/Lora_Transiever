#define BUTTON_PIN 18
char msgA = 'A';
char msgB = 'B';

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(22, OUTPUT);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {  // button is pressed
    Serial1.print(msgA);
    digitalWrite(22, HIGH);
  } else {
    Serial1.print(msgB);
    digitalWrite(22, LOW);
  }
  delay(750);
}
