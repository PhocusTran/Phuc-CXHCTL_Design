#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <map>
#include <PubSubClient.h> // MQTT
#include <WiFiClient.h>  // MQTT

#define ESP_NAME "Fanuc TC"
#define ESP_ID   "esp32_1"

// Pin definitions for 74HC595s
#define CLOCK_PIN 4  // SH_CP - 9
#define DATA_PIN  5 // DS - 10
#define LATCH_PIN 6 // ST_CP - 11

// Define number of shift registers
#define NUM_SHIFT_REGISTERS 6

// WiFi credentials
const char* ssid = "HCTL";
const char* password = "123456789HCTL";
const int udpPort = 8888;
WiFiUDP udp;

// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

const char *mqtt_topic = "esp32_1/relay_control";
const char *mqtt_status_topic = "esp32_1/status";

// MQTT Client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

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
    <button class="button" onclick="sendCommand('resetkey')">RESET</button>
    <button class="button" onclick="sendCommand('helpkey')">HELP<br>UP</button>
    <button class="button" onclick="sendCommand('deletekey')">DELETE<br>DOWN</button>
    <button class="button" onclick="sendCommand('inputkey')">INPUT<br>UP</button>
    <button class="button" onclick="sendCommand('cankey')">CAN<br>DOWN</button>
    <button class="button" onclick="sendCommand('insertkey')">INSERT</button>
    <button class="button" onclick="sendCommand('shiftkey')">SHIFT</button>
    <button class="button" onclick="sendCommand('alterkey')">ALTER</button>
    <button class="button" onclick="sendCommand('offsetkey')">OFFSET</button>
    <button class="button" onclick="sendCommand('graphkey')">CSTM/GR</button>
    <button class="button" onclick="sendCommand('progkey')">PROG</button>
    <button class="button" onclick="sendCommand('messagekey')">MESSAGE<br>START</button>
    <button class="button" onclick="sendCommand('poskey')">POS</button>
    <button class="button" onclick="sendCommand('systemkey')">SYSTEM</button>
    <button class="button" onclick="sendCommand('pageupkey')">PAGE<br>UP</button>
    <button class="button" onclick="sendCommand('pagedownkey')">PAGE<br>DOWN</button>
    <button class="button" onclick="sendCommand('upkey')">UP</button>
    <button class="button" onclick="sendCommand('downkey')">DOWN</button>
    <button class="button" onclick="sendCommand('leftkey')">LEFT</button>
    <button class="button" onclick="sendCommand('rightkey')">RIGHT</button>
    <button class="button" onclick="sendCommand('mkey')">.<br>/</button>
    <button class="button" onclick="sendCommand('tkey')">-<br>+</button>
    <button class="button" onclick="sendCommand('key0')">0<br>*</button>
    <button class="button" onclick="sendCommand('key1')">1<br>,</button>
    <button class="button" onclick="sendCommand('key2')">2<br>#</button>
    <button class="button" onclick="sendCommand('key3')">3<br>=</button>
    <button class="button" onclick="sendCommand('key4')">4<br>[</button>
    <button class="button" onclick="sendCommand('key5')">5<br>]</button>
    <button class="button" onclick="sendCommand('key6')">6<br>SP</button>
    <button class="button" onclick="sendCommand('key7')">7<br>A</button>
    <button class="button" onclick="sendCommand('key8')">8<br>B</button>
    <button class="button" onclick="sendCommand('key9')">9<br>D<</button>
    <button class="button" onclick="sendCommand('eobkey')">EOB<br>E</button>
    <button class="button" onclick="sendCommand('wvkey')">W<br>V</button>
    <button class="button" onclick="sendCommand('uhkey')">U<br>H</button>
    <button class="button" onclick="sendCommand('tjkey')">T<br>J</button>
    <button class="button" onclick="sendCommand('skkey')">S<br>K</button>
    <button class="button" onclick="sendCommand('mikey')">M<br>I</button>
    <button class="button" onclick="sendCommand('flkey')">F<br>L</button>
    <button class="button" onclick="sendCommand('zykey')">Z<br>Y</button>
    <button class="button" onclick="sendCommand('xckey')">X<br>C</button>
    <button class="button" onclick="sendCommand('grkey')">G<br>R</button>
    <button class="button" onclick="sendCommand('nqkey')">N<br>Q</button>
    <button class="button" onclick="sendCommand('opkey')">O<br>P</button>
    <button class="button" onclick="sendCommand('f1key')">F1<br>LEFT</button>
    <button class="button" onclick="sendCommand('f2key')">F2</button>
    <button class="button" onclick="sendCommand('f3key')">F3</button>
    <button class="button" onclick="sendCommand('f4key')">F4</button>
    <button class="button" onclick="sendCommand('f5key')">F5</button>
    <button class="button" onclick="sendCommand('f6key')">F6</button>
    <button class="button" onclick="sendCommand('f7key')">F7<br>RIGHT</button>
    <script>
        function sendCommand(command) {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", "/button?name=" + command, true);
            xhr.send();
        }
    </script>
</body>
</html>
)=====";

void updateRelays();
void connectToMQTT();

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
  Serial.println("Server started");

  // Initialize MQTT
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback); // Hàm xử lý tin nhắn MQTT

  connectToMQTT(); // Kết nối đến MQTT broker
}

void loop() {
  server.handleClient();
  mqtt_client.loop(); // Duy trì kết nối MQTT

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

  if (!mqtt_client.connected()) {
    connectToMQTT();
  }
}

void handleRoot() {
  String html = MAIN_page;
  server.send(200, "text/html", html);
}

void handleButton() {
  if (server.hasArg("name")) {
    String buttonName = server.arg("name");
    Serial.println("Button pressed on web: " + buttonName);

    // Publish the button name to the MQTT topic
    String message = buttonName;
    mqtt_client.publish(mqtt_status_topic, message.c_str()); // Publish to the STATUS topic
    mqtt_client.publish(mqtt_topic, message.c_str());
    server.send(200, "text/plain", "OK - Sent to MQTT");
  } else {
    server.send(400, "text/plain", "Missing button name");
  }
}

// MQTT callback function - this is where you handle incoming MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

    // Check if the message is for this device
    if (String(topic) == mqtt_topic) {
        handleRelayControl(message); // Call the function to control the relay
    }
}

void handleRelayControl(String buttonName) {
    // Check if the message is for the special function keys
  if (buttonName == "f5key") {
    digitalWrite(F5_KEY, HIGH);
    delay(200);
    digitalWrite(F5_KEY, LOW);
  } else if (buttonName == "f6key") {
    digitalWrite(F6_KEY, HIGH);
    delay(200);
    digitalWrite(F6_KEY, LOW);
  } else if (buttonName == "f7key") {
    digitalWrite(F7_KEY, HIGH);
    delay(200);
    digitalWrite(F7_KEY, LOW);
  } else if (buttonPins.count(buttonName)) { // Check if message is a valid button
        activateRelay(buttonPins[buttonName]);
  } else {
    Serial.println("Unknown command: " + buttonName);
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

// Function to connect to MQTT broker
void connectToMQTT() {
  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic); // Subscribe to the topic
      Serial.println("Subscribed to topic: " + String(mqtt_topic));

    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 5 seconds.");
      delay(5000);
        }
  }
}
