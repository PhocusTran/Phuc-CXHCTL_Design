#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>

// Pin definitions for 74HC595s
#define DATA_PIN  10 // DS
#define LATCH_PIN 11 // ST_CP
#define CLOCK_PIN 9  // SH_CP

// Define number of shift registers
#define NUM_SHIFT_REGISTERS 2

// WiFi credentials
const char* ssid = "HCTL_DEMO";
const char* password = "123456789HCTL";

WebServer server(80);
unsigned int relayState = 0; // Use unsigned int for 16 relays


void setup() {
  Serial.begin(115200);
  // Set pin modes
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  // Initialize all relays OFF.
  updateRelays();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
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
    for (int i = 0; i < NUM_SHIFT_REGISTERS * 8; i++) { // Up to 16 relays
        html += "<button onclick=\"activateRelay(" + String(i) + ")\">Relay " + String(i) + " (Q" + String(i) + ")</button><br>";
    }
    html += "<script>function activateRelay(relayNum){fetch('/relay?relayNum=' + relayNum)}</script>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleRelay() {
  if (server.hasArg("relayNum")) {
    int relayNum = server.arg("relayNum").toInt();
    if (relayNum >= 0 && relayNum < NUM_SHIFT_REGISTERS * 8) {
      activateRelay(relayNum);
      server.send(200, "text/html", "");
    } else {
      server.send(400, "text/plain", "Invalid relay number");
    }
  } else {
    server.send(400, "text/plain", "Missing relay number");
  }
}

void activateRelay(int relayNum) {
  relayState |= (1 << relayNum);
  updateRelays();
  delay(50);
  relayState &= ~(1 << relayNum);
  updateRelays();
}

void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
    // Send data for each shift register
    for (int i = NUM_SHIFT_REGISTERS; i >= 0; i--) {
        shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (relayState >> (i * 8)) & 0xFF);
    }
  digitalWrite(LATCH_PIN, HIGH);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(myClockPin, LOW);
    digitalWrite(myDataPin, (myDataOut >> i) & 1);
    digitalWrite(myClockPin, HIGH);
  }
}

void loop() {
  server.handleClient();
}
