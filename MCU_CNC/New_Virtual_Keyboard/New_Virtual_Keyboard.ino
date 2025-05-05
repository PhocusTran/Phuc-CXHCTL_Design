#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>

// Pin definitions for 74HC595 and relays
#define DATA_PIN  10
#define LATCH_PIN 11
#define CLOCK_PIN 9

// WiFi credentials
const char* ssid = "HCTL";
const char* password = "123456789HCTL";

WebServer server(80);
byte relayState = 0; // All relays OFF initially

// Relay names (adjust as needed)
const char* relayNames[] = {"Auto", "F01", "MDI", "Home", "X", "MPG", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
const int numRelays = sizeof(relayNames) / sizeof(relayNames[0]);


void setup() {
  Serial.begin(115200);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // Initialize all relays OFF.
  updateRelays();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/relay", handleRelay);
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = "<html><head><title>Relay Control</title></head><body>";
  html += "<h1>Relay Control</h1>";
  for (int i = 0; i < numRelays; i++) {
    html += "<button onclick=\"toggleRelay(" + String(i) + ")\">" + String(relayNames[i]) + "</button><br>";
  }
  html += "<script>function toggleRelay(relayNum){fetch('/relay?relayNum=' + relayNum)}</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleRelay() {
  if (server.hasArg("relayNum")) {
    int relayNum = server.arg("relayNum").toInt();
    if (relayNum >= 0 && relayNum < numRelays) {
      toggleRelay(relayNum);
      server.send(200, "text/plain", "OK"); // Simplified response
    } else {
      server.send(400, "text/plain", "Invalid relay number");
    }
  } else {
    server.send(400, "text/plain", "Missing relay number");
  }
}

void toggleRelay(int relayNum) {
  setRelay(relayNum, true);  // Turn ON
  delay(50);              // Wait 50ms
  setRelay(relayNum, false); // Turn OFF
}



// Function to set a single relay's state
void setRelay(int relayNum, bool state) {
  if (relayNum >= 0 && relayNum < numRelays) {  // Check bounds
    if (state) {
      relayState |= (1 << relayNum);
    } else {
      relayState &= ~(1 << relayNum);
    }
    updateRelays();
  }
}



void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relayState);
  digitalWrite(LATCH_PIN, HIGH);
}


// Correct shiftOut signature
void shiftOut(int myDataPin, int myClockPin, byte bitOrder, byte val) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(myClockPin, LOW);
    if (bitOrder == MSBFIRST) {
      digitalWrite(myDataPin, (val >> i) & 1);
    } else {
      digitalWrite(myDataPin, (val >> (7 - i)) & 1); // LSBFIRST
    }
    digitalWrite(myClockPin, HIGH);
  }
}



void loop() {
  server.handleClient();
}
