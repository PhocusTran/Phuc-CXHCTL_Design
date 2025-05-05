#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <map>
#include <Arduino.h>

// Pin definitions for 74HC595s
#define CLOCK_PIN 4  // SH_CP - 9
#define DATA_PIN  5 // DS - 10
#define LATCH_PIN 6 // ST_CP - 11

// Define number of shift registers
#define NUM_SHIFT_REGISTERS 6

// WiFi credentials
const char* ssid = "HCTL_AP";
const char* password = "123456789HCTL";
const int udpPort = 8888;
WiFiUDP udp;

WebServer server(80);
uint8_t relayState[NUM_SHIFT_REGISTERS] = {0}; // Mảng lưu trạng thái từng IC
const int F5_KEY = 7;  // Sửa chân
const int F6_KEY = 8;  // Sửa chân
const int F7_KEY = 9;  // Sửa chân
bool f5keyState = false;
bool f6keyState = false;
bool f7keyState = false;

// Mapping button names to relay numbers.  Adjust these to your wiring!
std::map<String, int> buttonPins =
{
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
  {"f4key", 47}
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
    <button class="button" onclick="sendCommand('/pagedownkey')">PAGE<br>DOWN</button>
    <button class="button" onclick="sendCommand('/upkey')">UP</button>
    <button class="button" onclick="sendCommand('/downkey')">DOWN</button>
    <button class="button" onclick="sendCommand('/leftkey')">LEFT</button>
    <button class="button" onclick="sendCommand('/rightkey')">RIGHT</button>
    <button class="button" onclick="sendCommand('/mkey')">.<br>/</button>
    <button class="button" onclick="sendCommand('/tkey')">-<br>+</button>
    <button class="button" onclick="sendCommand('/key0')">0<br>*</button>
    <button class="button" onclick="sendCommand('/key1')">1<br>,</button>
    <button class="button" onclick="sendCommand('/key2')">2<br>#</button>
    <button class="button" onclick="sendCommand('/key3')">3<br>=</button>
    <button class="button" onclick="sendCommand('/key4')">4<br>[</button>
    <button class="button" onclick="sendCommand('/key5')">5<br>]</button>
    <button class="button" onclick="sendCommand('/key6')">6<br>SP</button>
    <button class="button" onclick="sendCommand('/key7')">7<br>A</button>
    <button class="button" onclick="sendCommand('/key8')">8<br>B</button>
    <button class="button" onclick="sendCommand('/key9')">9<br>D<</button>
    <button class="button" onclick="sendCommand('/eobkey')">EOB<br>E</button>
    <button class="button" onclick="sendCommand('/wvkey')">W<br>V</button>
    <button class="button" onclick="sendCommand('/uhkey')">U<br>H</button>
    <button class="button" onclick="sendCommand('/tjkey')">T<br>J</button>
    <button class="button" onclick="sendCommand('/skkey')">S<br>K</button>
    <button class="button" onclick="sendCommand('/mikey')">M<br>I</button>
    <button class="button" onclick="sendCommand('/flkey')">F<br>L</button>
    <button class="button" onclick="sendCommand('/zykey')">Z<br>Y</button>
    <button class="button" onclick="sendCommand('/xckey')">X<br>C</button>
    <button class="button" onclick="sendCommand('/grkey')">G<br>R</button>
    <button class="button" onclick="sendCommand('/nqkey')">N<br>Q</button>
    <button class="button" onclick="sendCommand('/opkey')">O<br>P</button>
    <button class="button" onclick="sendCommand('/f1key')">F1<br>LEFT</button>
    <button class="button" onclick="sendCommand('/f2key')">F2</button>
    <button class="button" onclick="sendCommand('/f3key')">F3</button>
    <button class="button" onclick="sendCommand('/f4key')">F4</button>
    <button class="button" onclick="sendCommand('/f5key')">F5</button>
    <button class="button" onclick="sendCommand('/f6key')">F6</button>
    <button class="button" onclick="sendCommand('/f7key')">F7<br>RIGHT</button>
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

void handleButtonPress(String buttonName) {
  Serial.println("Button pressed: " + buttonName);

  if (buttonName == "f5key") {
    server.send(200, "text/plain", "F5");
    digitalWrite(F5_KEY, HIGH);
    delay(200);
    digitalWrite(F5_KEY, LOW);
  } else if (buttonName == "f6key") {
    server.send(200, "text/plain", "F6");
    digitalWrite(F6_KEY, HIGH);
    delay(200);
    digitalWrite(F6_KEY, LOW);
  } else if (buttonName == "f7key") {
    server.send(200, "text/plain", "F7");
    digitalWrite(F7_KEY, HIGH);
    delay(200);
    digitalWrite(F7_KEY, LOW);
  } else if (buttonPins.count(buttonName)) {
    activateRelay(buttonPins[buttonName]);
  }
}

void handleTemperature() {
  float temp_celsius = temperatureRead(); // Read the temperature
  String temp_str = String(temp_celsius, 2); // Convert to string with 2 decimal places
  Serial.println("ESP32 Temperature: " + temp_str);
  server.send(200, "text/plain", temp_str); // Send as plain text
}

void setup() {
  Serial.begin(115200);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(F5_KEY, OUTPUT);
  pinMode(F6_KEY, OUTPUT);
  pinMode(F7_KEY, OUTPUT);

  updateRelays(); // Initialize relays OFF
  digitalWrite(F5_KEY, LOW);
  digitalWrite(F6_KEY, LOW);
  digitalWrite(F7_KEY, LOW);

  // Local IP
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Access Point
  //  if (!WiFi.softAP(ssid, password)) {
  //    log_e("Soft AP creation failed.");
  //    while (1);
  //  }
  //  IPAddress myIP = WiFi.softAPIP();
  //  Serial.print("AP IP address: ");
  //  Serial.println(myIP);
  //  server.begin();

  udp.begin(udpPort);

  server.on("/", handleRoot);
  server.on("/button", HTTP_GET, handleButton); // Single handler for all buttons
  server.on("/temperature", HTTP_GET, handleTemperature); // New endpoint for temperature

  server.begin();
  Serial.println("Server started");
}

// Local IP
void loop() {
  server.handleClient();
  // Gửi địa chỉ IP qua UDP mỗi 5 giây
  static unsigned long last_udp_sent = 0;
  if (millis() - last_udp_sent >= 5000) {
    last_udp_sent = millis();
    String ip_str = WiFi.localIP().toString();
    udp.beginPacket(IPAddress(255, 255, 255, 255), udpPort);
    udp.print(ip_str);
    udp.endPacket();
    Serial.println("Sent IP via UDP: " + ip_str); // In ra Serial Monitor để debug
  }
}

// Access Point
//void loop() {
//  server.handleClient();
//  // Gửi địa chỉ IP qua UDP mỗi 5 giây
//  static unsigned long last_udp_sent = 0;
//  if (millis() - last_udp_sent >= 5000) {
//    last_udp_sent = millis();
//    String ip_str = WiFi.softAPIP().toString(); // Sử dụng softAPIP() để lấy IP của AP
//    Serial.print("IP to send: ");
//    Serial.println(ip_str);
//    udp.beginPacket(IPAddress(255, 255, 255, 255), udpPort);
//    udp.print(ip_str);
//    udp.endPacket();
//    Serial.println("Sent IP via UDP: " + ip_str); // In ra Serial Monitor để debug
//  }
//}

void handleRoot() {
  String html = MAIN_page;
  server.send(200, "text/html", html);
}

void handleButton() {
  if (server.hasArg("name")) {
    String buttonName = server.arg("name");
    handleButtonPress(buttonName);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing button name");
  }
}

void activateRelay(int relayNum) {
  int shiftReg = relayNum / 8;   // Tính toán IC nào sẽ điều khiển relay này
  int pinInReg = relayNum % 8; // Tính toán chân relay trong IC đó
  relayState[shiftReg] |= (1 << pinInReg);   // Bật relay tương ứng
  updateRelays();
  delay(200);
  relayState[shiftReg] &= ~(1 << pinInReg); // Tắt relay tương ứng
  updateRelays();
}

void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
  // Send data for each shift register
  for (int i = NUM_SHIFT_REGISTERS - 1; i >= 0; i--) {
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, relayState[i]);
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
