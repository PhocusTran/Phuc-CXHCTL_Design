#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <WiFiUdp.h>
#include <map>

// Pin definitions for 74HC595s
#define CLOCK_PIN 9  // SH_CP - 4
#define DATA_PIN  10 // DS - 5
#define LATCH_PIN 11 // ST_CP - 6

// Define number of shift registers
#define NUM_SHIFT_REGISTERS 6

// WiFi credentials
const char* ssid = "HCTL_DEMO";
const char* password = "123456789HCTL";
const int udpPort = 8888; 
WiFiUDP udp;

WebServer server(80);
unsigned int relayState = 0; 

// Mapping button names to relay numbers.  Adjust these to your wiring!
std::map<String, int> buttonPins = {
  {"resetkey", 0},
  {"helpkey", 1},
  {"deletekey", 2},
  {"inputkey", 3},
  {"cankey", 4},
  {"insertkey", 5},
  {"shiftkey", 6},
  {"alterkey", 7},
  {"offsetkey", 8},
  {"graphkey", 9},
  {"progkey", 10},
  {"messagekey", 11},
  {"poskey", 12},
  {"systemkey", 13},
  {"pageupkey", 14},
  {"pagedownkey", 15},
  {"upkey", 16},
  {"downkey", 17},
  {"leftkey", 18},
  {"rightkey", 19},
  {"mkey", 20},
  {"tkey", 21},
  {"key0", 22},
  {"key1", 23},
  {"key2", 24},
  {"key3", 25},
  {"key4", 26},
  {"key5", 27},
  {"key6", 28},
  {"key7", 29},
  {"key8", 30},
  {"key9", 31},
  {"eobkey", 32},
  {"wvkey", 33},
  {"uhkey", 34},
  {"tjkey", 35},
  {"skkey", 36},
  {"mikey", 37},
  {"flkey", 38},
  {"zykey", 39},
  {"xckey", 40},
  {"grkey", 41},
  {"nqkey", 42},
  {"opkey", 43},
  {"f1key", 44},
  {"f2key", 45},
  {"f3key", 46},
  {"f4key", 47},
  {"f5key", 48},
  {"f6key", 49},
  {"f7key", 50}
};

// HTML webpage (đã sửa lại để thêm các hàm xử lý)
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
    <h1>CNC Keyboard Control</h1>
    <button class="button" onclick="sendCommand('/resetkey')">RESET</button>
    <button class="button" onclick="sendCommand('/helpkey')">HELP<br>UP</button>
    <button class="button" onclick="sendCommand('/deletekey')">DELETE<br>DOWN</button>
    <button class="button" onclick="sendCommand('/inputkey')">INPUT<br>UP</button>
    <button class="button" onclick="sendCommand('/cankey')">CAN<br>DOWN</button>
    <button class="button" onclick="sendCommand('/insertkey')">INSERT</button>
    <button class="button" onclick="sendCommand('/shiftkey')">SHIFT</button>
    <button class="button" onclick="sendCommand('/alterkey')">ALTER</button>
    <button class="button" onclick="sendCommand('/offsetkey')">OFFSET</button>
    <button class="button" onclick="sendCommand('/graphkey')">CSTM/GR</button>
    <button class="button" onclick="sendCommand('/progkey')">PROG</button>
    <button class="button" onclick="sendCommand('/messagekey')">MESSAGE<br>START</button>
    <button class="button" onclick="sendCommand('/poskey')">POS</button>
    <button class="button" onclick="sendCommand('/systemkey')">SYSTEM</button>
    <button class="button" onclick="sendCommand('/pageupkey')">PAGE<br>UP</button>
    <button class="button" onclick="sendCommand('/pagedownkey')">PAGE<b>DOWN<br>PARAM</button>
    <button class="button" onclick="sendCommand('/upkey')">RESET</button>
    <button class="button" onclick="sendCommand('/downkey')">CURSOR<br>UP</button>
    <button class="button" onclick="sendCommand('/leftkey')">CURSOR<br>DOWN</button>
    <button class="button" onclick="sendCommand('/rightkey')">PAGE<br>UP</button>
    <button class="button" onclick="sendCommand('/mkey')">PAGE<br>DOWN</button>
    <button class="button" onclick="sendCommand('/tkey')">ALTER</button>
    <button class="button" onclick="sendCommand('/key0')">INSERT</button>
    <button class="button" onclick="sendCommand('/key1')">DELETE</button>
    <button class="button" onclick="sendCommand('/key2')">/.#<br>EOB</button>
    <button class="button" onclick="sendCommand('/key3')">CAN</button>
    <button class="button" onclick="sendCommand('/key4')">INPUT</button>
    <button class="button" onclick="sendCommand('/key5')">OUTPUT<br>START</button>
    <button class="button" onclick="sendCommand('/key6')">POS</button>
    <button class="button" onclick="sendCommand('/key7')">PROGRAM</button>
    <button class="button" onclick="sendCommand('/key8')">MENU<br>OFFSET</button>
    <button class="button" onclick="sendCommand('/key9')">DGNOS<br>PARAM</button>
    <button class="button" onclick="sendCommand('/eobkey')">RESET</button>
    <button class="button" onclick="sendCommand('/wvkey')">CURSOR<br>UP</button>
    <button class="button" onclick="sendCommand('/uhkey')">CURSOR<br>DOWN</button>
    <button class="button" onclick="sendCommand('/tjkey')">PAGE<br>UP</button>
    <button class="button" onclick="sendCommand('/skkey')">PAGE<br>DOWN</button>
    <button class="button" onclick="sendCommand('/mikey')">ALTER</button>
    <button class="button" onclick="sendCommand('/flkey')">INSERT</button>
    <button class="button" onclick="sendCommand('/zykey')">DELETE</button>
    <button class="button" onclick="sendCommand('/zykey')">/.#<br>EOB</button>
    <button class="button" onclick="sendCommand('/xckey')">CAN</button>
    <button class="button" onclick="sendCommand('/grkey')">INPUT</button>
    <button class="button" onclick="sendCommand('/nqkey')">OUTPUT<br>START</button>
    <button class="button" onclick="sendCommand('/opkey')">POS</button>
    <button class="button" onclick="sendCommand('/f1key')">PROGRAM</button>
    <button class="button" onclick="sendCommand('/f2key')">MENU<br>OFFSET</button>
    <button class="button" onclick="sendCommand('/f3key')">DGNOS<br>PARAM</button>
    <button class="button" onclick="sendCommand('/f4key')">PROGRAM</button>
    <button class="button" onclick="sendCommand('/f5key')">MENU<br>OFFSET</button>
    <button class="button" onclick="sendCommand('/f6key')">DGNOS<br>PARAM</button>
    <button class="button" onclick="sendCommand('/f7key')">DGNOS<br>PARAM</button>

    <script>
        function sendCommand(command) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", command, true);
            xhr.send();
        }
    </script>
</body>
</html>
)=====";


void setup() {
  Serial.begin(115200);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  updateRelays(); // Initialize relays OFF

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);

  server.on("/", handleRoot);
  server.on("/button", HTTP_GET, handleButton); // Single handler for all buttons
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long last_udp_sent = 0;
  if (millis() - last_udp_sent >= 5000) {
    last_udp_sent = millis();
    String ip_str = WiFi.localIP().toString();
    udp.beginPacket(IPAddress(255, 255, 255, 255), udpPort);
    udp.print(ip_str);
    udp.endPacket();
    Serial.println("Sent IP via UDP: " + ip_str); 
  }
}

void handleRoot() {
  String html = MAIN_page;
  server.send(200, "text/html", html);
}

void handleButton() {
  if (server.hasArg("name")) {
    String buttonName = server.arg("name");
    if (buttonPins.count(buttonName)) {
      activateRelay(buttonPins[buttonName]); // Activate the corresponding relay
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid button name");
    }
  } else {
    server.send(400, "text/plain", "Missing button name");
  }
}

void activateRelay(int relayNum) {
  relayState |= (1 << relayNum);
  updateRelays();
  delay(200);
  relayState &= ~(1 << relayNum);
  updateRelays();
}

void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
  for (int i = NUM_SHIFT_REGISTERS - 1; i >= 0; i--) { // Correct loop for 2 shift registers
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (relayState >> (i * 8)) & 0xFF);
  }
  digitalWrite(LATCH_PIN, HIGH);
}
