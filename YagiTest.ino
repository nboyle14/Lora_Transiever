#include <WiFi.h>
#include <time.h>

#define BUTTON_PIN 19       // Input pin for button
#define MOTOR_PIN 21         // Output pin for vibration motor
#define LED_PIN 23           // Output pin for LED

#define MAX_EVENTS 512       // Maximum number of events to track
unsigned long pressDurations[MAX_EVENTS];     // Store durations of button presses
unsigned long pressStartTimes[MAX_EVENTS];    // Store timestamps of when each press started
int eventCount = 0;         // Track how many button press events have been stored

bool lastButtonState = HIGH;   // Track last button state
bool isPressing = false;       // Track if button is currently pressed
unsigned long pressStartTime = 0;      // When the current press started
unsigned long lastActivityTime = 0;    // Last time button state changed

// Wi-Fi credentials for initial time sync
const char* ssid = "ScoopoHome";
const char* password = "BoyleScoopo#123";

// Timezone settings
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;

// Flags to prevent repeated sending
bool heartbeatSent = false;
bool sleepSent = false;

// Message header and ID
const byte HEADER[] = {0x00, 0x03, 0x6a, 0x04};


void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);  // LoRa via Serial1 on pins 16 (RX), 17 (TX)

  // Connect to Wi-Fi to sync time
  //WiFi.begin(ssid, password);
  
  // WiFi.begin("NUwave", WPA2_AUTH_PEAP, "boyle.ni@northeastern.edu", "boyle.ni@northeastern.edu", "%@Ng3liKaK.2028$$!");
  // while (WiFi.status() != WL_CONNECTED) {
  //   digitalWrite(LED_PIN, HIGH);
  //   delay(250);
  //   digitalWrite(LED_PIN, LOW);
  //   delay(250);
  // }

  // // Configure NTP time
  // configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

  // struct tm timeinfo;
  // while (!getLocalTime(&timeinfo)) {
  //   digitalWrite(MOTOR_PIN, HIGH);
  //   delay(250);
  //   digitalWrite(MOTOR_PIN, LOW);
  // }

  // // Disable Wi-Fi after sync
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
}

void loop() {
  bool currentState = digitalRead(BUTTON_PIN);  // Read button state

  // Detect state change and record event
  if (currentState != lastButtonState) {
    lastActivityTime = millis();

    if (currentState == LOW) {
      pressStartTime = millis();
      isPressing = true;
    } else if (isPressing) {
      unsigned long duration = millis() - pressStartTime;
      if (eventCount < MAX_EVENTS) {
        pressStartTimes[eventCount] = pressStartTime;
        pressDurations[eventCount] = duration;
        eventCount++;
      }
      isPressing = false;
    }

    delay(10); // Debounce
  }

  lastButtonState = currentState;

  // If no activity for 5 seconds and events exist, send sequence
  if (millis() - lastActivityTime > 5000 && eventCount > 0) {
    Serial1.write(HEADER, sizeof(HEADER));  // Send header
    Serial.write(HEADER, sizeof(HEADER));   // Mirror to Serial Monitor
    Serial1.println();  // FIXED: Newline after header to separate from data
    Serial.println();   // Mirror to Serial Monitor

    for (int i = 0; i < eventCount; i++) {
      char buffer[9];
      sprintf(buffer, "%08lX", pressDurations[i]);  // Format duration as 8-digit hex
      Serial1.print(buffer);
      Serial.print(buffer);
      Serial1.print(" ");
      Serial.print(" ");

      if (i < eventCount - 1) {
        // Calculate delay between this release and next press
        unsigned long endOfCurrent = pressStartTimes[i] + pressDurations[i];
        unsigned long startOfNext = pressStartTimes[i + 1];
        unsigned long gap = (startOfNext > endOfCurrent) ? startOfNext - endOfCurrent : 0;
        sprintf(buffer, "%08lX", gap);
        Serial1.print(buffer);
        Serial.print(buffer);
        Serial1.print(" ");
        Serial.print(" ");
      }
    }

    Serial1.println(); // End message
    Serial.println();  // Mirror to Serial Monitor
    eventCount = 0;    // Reset event count
  }

  // Process incoming LoRa messages
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');  // Read full message line
    msg.trim();
    
    Serial.print("Received: ");
    Serial.println(msg);
    
    // Skip header line (contains binary header bytes)
    if (msg.length() <= 4) {
      // Wait briefly for the actual data line
      unsigned long timeout = millis() + 100;
      while (!Serial1.available() && millis() < timeout) {
        delay(1);
      }
      if (Serial1.available()) {
        msg = Serial1.readStringUntil('\n');
        msg.trim();
        Serial.print("Received: ");
        Serial.println(msg);
      } else {
        return;
      }
    }

    if (msg.length() == 2) {
      byte code = strtol(msg.c_str(), NULL, 16);
      if (code == 0x41) heartBeat();  // If heartbeat signal received, play pattern   
      return;
    }

    int startIdx = 0;
    bool isPress = true;

    while (startIdx < msg.length()) {
      int spaceIdx = msg.indexOf(' ', startIdx);
      String token;

      if (spaceIdx == -1) {
        token = msg.substring(startIdx);
        startIdx = msg.length();
      } else {
        token = msg.substring(startIdx, spaceIdx);
        startIdx = spaceIdx + 1;
      }

      token.trim();
      unsigned long value = strtoul(token.c_str(), NULL, 16); // Convert hex to long

      if (isPress && value > 0) {
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(MOTOR_PIN, HIGH);
        delay(value);  // Activate motor/LED for press duration
        digitalWrite(LED_PIN, LOW);
        digitalWrite(MOTOR_PIN, LOW);
      } else {
        delay(value);  // Wait for gap duration
      }

      isPress = !isPress; // Alternate between press and gap
    }
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour == 12 && timeinfo.tm_min == 0 && !heartbeatSent) {
      Serial1.write(HEADER, sizeof(HEADER));  // Send full header
      Serial.write(HEADER, sizeof(HEADER));   // Mirror to Serial Monitor
      Serial1.write((byte)0x41);              // Send HEARTBEAT byte
      Serial.write((byte)0x41);               // Mirror to Serial Monitor
      heartbeatSent = true;
    }

    if (timeinfo.tm_hour == 22 && timeinfo.tm_min == 0 && !sleepSent) {
      Serial1.write(HEADER, sizeof(HEADER));  // Send full header
      Serial.write(HEADER, sizeof(HEADER));   // Mirror to Serial Monitor
      Serial1.write((byte)0x53);              // Send SLEEP byte
      Serial.write((byte)0x53);               // Mirror to Serial Monitor
      sleepSent = true;
    }

    // Reset daily flags
    if (timeinfo.tm_min != 0) {
      heartbeatSent = false;
      sleepSent = false;
    }
  }
}

// Heartbeat signal pattern for feedback
void heartBeat() {
  for (int x = 0; x < 4; x++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(MOTOR_PIN, HIGH);
    delay(450);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(MOTOR_PIN, LOW);
    delay(150);
  }
  for (int i = 0; i < 2; i++) {
    for (int t = 0; t < 3; t++) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(MOTOR_PIN, HIGH);
      delay(125);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(MOTOR_PIN, LOW);
      delay(40);
    }
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(MOTOR_PIN, HIGH);
    delay(725);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(MOTOR_PIN, LOW);
    delay(150);
  }
}