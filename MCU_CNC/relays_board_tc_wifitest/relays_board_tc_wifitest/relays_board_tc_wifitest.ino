#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <map>
#include <PubSubClient.h> // MQTT
#include <WiFiClient.h>  // MQTT
#include <EEPROM.h> // Thêm thư viện EEPROM

#define ESP_NAME "Fanuc TC"
#define ESP_ID   "esp32_1"

// Pin definitions for 74HC595s
#define CLOCK_PIN 4  // SH_CP - 9
#define DATA_PIN  5 // DS - 10
#define LATCH_PIN 6 // ST_CP - 11

// Define number of shift registers
#define NUM_SHIFT_REGISTERS 6

// WiFi credentials (mặc định, sẽ được ghi đè nếu có trong EEPROM)
const char* ssid = "HCTL_Office";
const char* password = "password";
const char* apSSID = "ESP32-Config"; // Tên mạng WiFi AP
const char* apPassword = "password";     // Mật khẩu mạng WiFi AP
const int udpPort = 8888;
WiFiUDP udp;

// Định nghĩa địa chỉ EEPROM để lưu SSID và password
#define SSID_ADDR 0
#define PASSWORD_ADDR 64 // Đảm bảo đủ khoảng trống sau SSID

// Kích thước tối đa cho SSID và password
#define MAX_SSID_LENGTH 63
#define MAX_PASSWORD_LENGTH 63

char saved_ssid[MAX_SSID_LENGTH + 1] = ""; // +1 để chứa ký tự null
char saved_password[MAX_PASSWORD_LENGTH + 1] = "";

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
<head>
    <title>CNC Keyboard Control</title>
    <style>
        /* Thêm CSS để định dạng form */
        .wifi-form {
            margin-bottom: 20px;
        }

        .wifi-form label {
            display: block;
            margin-bottom: 5px;
        }

        .wifi-form input[type="text"],
        .wifi-form input[type="password"] {
            width: 200px;
            padding: 5px;
            margin-bottom: 10px;
            border: 1px solid #ccc;
            border-radius: 4px;
            box-sizing: border-box; /* Để padding không làm thay đổi kích thước */
        }

        .wifi-form button {
            background-color: #4CAF50;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }

        .wifi-form button:hover {
            background-color: #45a049;
        }
    </style>
</head>
<body>
    <h1>CNC Keyboard Control</h1>

    <div class="wifi-form">
        <h2>WiFi Configuration</h2>
        <label for="ssid">SSID:</label>
        <input type="text" id="ssid" name="ssid"><br>
        <label for="password">Password:</label>
        <input type="password" id="password" name="password"><br>
        <button onclick="saveWifiConfig()">Save WiFi</button>
    </div>

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
        function saveWifiConfig() {
            var ssid = document.getElementById("ssid").value;
            var password = document.getElementById("password").value;

            var xhr = new XMLHttpRequest();
            xhr.open("POST", "/saveWifi", true);
            xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");

            xhr.onload = function() {
                if (xhr.status == 200) {
                    alert("WiFi settings saved.  Rebooting...");
                    var xhrReboot = new XMLHttpRequest();
                    xhrReboot.open("GET", "/reboot", true);
                    xhrReboot.send();
                } else {
                    alert("Error saving WiFi settings.");
                }
            };

            xhr.send("ssid=" + encodeURIComponent(ssid) + "&password=" + encodeURIComponent(password));
        }

        function loadSavedWifiConfig() {
            document.getElementById("ssid").value = ssid;
            document.getElementById("password").value = password;
        }
    </script>
</body>
</html>
)=====";

void updateRelays();
void connectToMQTT();
bool startWiFi();

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

  EEPROM.begin(512);
  loadWiFiCredentials();

  if (!startWiFi()) {
    Serial.println("Failed to connect to WiFi. Starting Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/button", HTTP_GET, handleButton);
  server.on("/saveWifi", HTTP_POST, handleSaveWifi);
  server.on("/reboot", HTTP_GET, handleReboot);

  server.on("/", [&]() {
    String html = MAIN_page;
    html.replace("<?php echo $ssid; ?>", saved_ssid);
    html.replace("<?php echo $password; ?>", saved_password);
    server.send(200, "text/html", html);
  });

  server.begin();
  Serial.println("Server started");

  if (WiFi.getMode() == WIFI_STA) {
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setCallback(mqttCallback);
    connectToMQTT();
  }
}

void loop() {
  server.handleClient();
  if (WiFi.getMode() == WIFI_STA) {
      mqtt_client.loop();

      static unsigned long last_udp_sent = 0;
      if (millis() - last_udp_sent >= 5000) {
        last_udp_sent = millis();
        String ip_str = WiFi.localIP().toString();
        udp.beginPacket(IPAddress(255, 255, 255, 255), udpPort);
        udp.print(ip_str);
        udp.endPacket();
        Serial.println("Sent IP via UDP: " + ip_str);
      }
      if (!mqtt_client.connected()) {
        connectToMQTT();
      }
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Attempting to reconnect with saved credentials.");
        if (strlen(saved_ssid) > 0) {
          WiFi.begin(saved_ssid, saved_password);
        } else {
          WiFi.begin(ssid, password);
        }
        delay(5000);
      }
    }
}

void handleRoot() {
  String html = MAIN_page;
  html.replace("<?php echo $ssid; ?>", saved_ssid);
  html.replace("<?php echo $password; ?>", saved_password);
  server.send(200, "text/html", html);
}

void handleButton() {
  if (server.hasArg("name")) {
    String buttonName = server.arg("name");
    Serial.println("Button pressed on web: " + buttonName);

    String message = buttonName;
    mqtt_client.publish(mqtt_status_topic, message.c_str());
    mqtt_client.publish(mqtt_topic, message.c_str());
    server.send(200, "text/plain", "OK - Sent to MQTT");
  } else {
    server.send(400, "text/plain", "Missing button name");
  }
}

void handleSaveWifi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");

    if (new_ssid.length() > MAX_SSID_LENGTH || new_password.length() > MAX_PASSWORD_LENGTH) {
      server.send(400, "text/plain", "SSID or password too long.");
      return;
    }

    saveWiFiCredentials(new_ssid.c_str(), new_password.c_str());

    server.send(200, "text/plain", "WiFi settings saved. Rebooting...");
  } else {
    server.send(400, "text/plain", "Missing SSID or password");
  }
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(100);
  ESP.restart();
}

void saveWiFiCredentials(const char* ssid, const char* password) {
  Serial.println("Saving WiFi credentials to EEPROM...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);

  for (int i = 0; i < MAX_SSID_LENGTH + 1; ++i) {
    EEPROM.write(SSID_ADDR + i, 0);
  }
  for (int i = 0; i < MAX_PASSWORD_LENGTH + 1; ++i) {
    EEPROM.write(PASSWORD_ADDR + i, 0);
  }

  for (int i = 0; i < strlen(ssid); ++i) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
  }
  EEPROM.write(SSID_ADDR + strlen(ssid), '\0');

  for (int i = 0; i < strlen(password); ++i) {
    EEPROM.write(PASSWORD_ADDR + i, password[i]);
  }
  EEPROM.write(PASSWORD_ADDR + strlen(password), '\0');

  EEPROM.commit();
  Serial.println("WiFi credentials saved to EEPROM");

  delay(100);
  ESP.restart();
}

void loadWiFiCredentials() {
  for (int i = 0; i < MAX_SSID_LENGTH; ++i) {
    saved_ssid[i] = EEPROM.read(SSID_ADDR + i);
    if (saved_ssid[i] == '\0') break;
  }
  saved_ssid[MAX_SSID_LENGTH] = '\0';

  for (int i = 0; i < MAX_PASSWORD_LENGTH; ++i) {
    saved_password[i] = EEPROM.read(PASSWORD_ADDR + i);
    if (saved_password[i] == '\0') break;
  }
  saved_password[MAX_PASSWORD_LENGTH] = '\0';

  Serial.print("Loaded SSID: ");
  Serial.println(saved_ssid);
  Serial.print("Loaded Password: ");
  Serial.println(saved_password);
}

bool startWiFi() {
  WiFi.mode(WIFI_STA);
  if (strlen(saved_ssid) > 0) {
    Serial.print("Connecting to saved WiFi: ");
    Serial.println(saved_ssid);
    WiFi.begin(saved_ssid, saved_password);
  } else {
    Serial.print("Connecting to default WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
  }

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("WiFi connection failed.");
    return false;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

    if (String(topic) == mqtt_topic) {
        handleRelayControl(message);
    }
}

void handleRelayControl(String buttonName) {
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
  } else if (buttonPins.count(buttonName)) {
        activateRelay(buttonPins[buttonName]);
  } else {
    Serial.println("Unknown command: " + buttonName);
  }
}

void activateRelay(int relayNum) {
  int shiftReg = relayNum / 8;
  int pinInReg = relayNum % 8;
  relayState[shiftReg] |= (1 << pinInReg);
  updateRelays();
  delay(200);
  relayState[shiftReg] &= ~(1 << pinInReg);
  updateRelays();
}

void updateRelays() {
  digitalWrite(LATCH_PIN, LOW);
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

void connectToMQTT() {
  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic);
      Serial.println("Subscribed to topic: " + String(mqtt_topic));

    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 5 seconds.");
      delay(5000);
        }
  }
}
