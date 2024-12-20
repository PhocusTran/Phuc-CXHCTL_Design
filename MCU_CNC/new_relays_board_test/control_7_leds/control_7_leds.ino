#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>

// Pin definitions for 74HC595 and relays
#define DATA_PIN  10
#define LATCH_PIN 11
#define CLOCK_PIN 9
#define RELAY_1_PIN  5
#define RELAY_2_PIN  18
#define RELAY_3_PIN  19
#define RELAY_4_PIN  21
#define RELAY_5_PIN  22
#define RELAY_6_PIN  23
#define RELAY_7_PIN  25

// WiFi credentials
const char* ssid = "Ky Quang";
const char* password = "Quang345678";

WebServer server(80);
byte relayState = 0; // All relays OFF initially

void setup() {
  Serial.begin(115200);
  // Set pin modes
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(RELAY_1_PIN, OUTPUT);
  pinMode(RELAY_2_PIN, OUTPUT);
  pinMode(RELAY_3_PIN, OUTPUT);
  pinMode(RELAY_4_PIN, OUTPUT);
  pinMode(RELAY_5_PIN, OUTPUT);
  pinMode(RELAY_6_PIN, OUTPUT);
  pinMode(RELAY_7_PIN, OUTPUT);

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
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  String html = "<html><head><title>Relay Control</title></head><body>";
  html += "<h1>Relay Control</h1>";
  for (int i = 1; i <= 7; i++) {
    html += "<button onclick=\"toggleRelay(" + String(i) + ")\">Relay " + String(i) + " (Q" + String(i) + ")</button><br>";
  }
  html += "<script>function toggleRelay(relayNum){fetch('/relay?relayNum=' + relayNum)}</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


void handleRelay() {
  if (server.hasArg("relayNum")) {
    int relayNum = server.arg("relayNum").toInt();
    if (relayNum >= 1 && relayNum <= 7) {
      setRelay(relayNum, !getRelayState(relayNum)); // Toggle relay state
      server.send(200, "text/html", "");
    } else {
      server.send(400, "text/plain", "Invalid relay number");
    }
  } else {
    server.send(400, "text/plain", "Missing relay number");
  }
}

void handleNotFound() {
  Serial.println("Not Found");
  server.send(404, "text/plain", "Not found");
}

// Function to set a single relay's state
void setRelay(int relayNum, bool state) {
  if (relayNum >=1 && relayNum <=7){
    if (state) {
        relayState |= (1 << relayNum);
    } else {
        relayState &= ~(1 << relayNum);
    }
    updateRelays();
  }
}


// Function to get a single relay's state
bool getRelayState(int relayNum) {
  return (relayState >> relayNum) & 1;
}

void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relayState);
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
