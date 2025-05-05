#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <map>
#include <PubSubClient.h> // MQTT
#include <WiFiClient.h>  // MQTT

// ----- Cấu hình cho ESP32 -----
#define ESP_NAME "Fanuc OT"
#define ESP_ID   "esp32_2"

// Thông tin WiFi
const char* ssid = "HCTL";
const char* password = "123456789HCTL";
const int udpPort = 8889;
WiFiUDP udp;

// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "emqx"; // Thay bằng username của bạn
const char *mqtt_password = "public"; // Thay bằng password của bạn
const int mqtt_port = 1883; // Sử dụng cổng không mã hóa để đơn giản hóa

const char *mqtt_topic = "esp32_2/relay_control"; // Topic cho điều khiển relay
const char *mqtt_status_topic = "esp32_2/status"; // Topic cho báo cáo trạng thái

// MQTT Client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

WebServer server(80);

// Khai báo chân cho các nút
const int RESET_KEY = 0;
const int CURSORUP_KEY = 1;
const int CURSORDOWN_KEY = 2;
const int PAGEUP_KEY = 3;
const int PAGEDOWN_KEY = 4;
const int ALTER_KEY = 5;
const int INSERT_KEY = 6;
const int DELETE_KEY = 7;
const int EOB_KEY = 8;
const int CAN_KEY = 9;
const int INPUT_KEY = 10;
const int OUTPUT_KEY = 11;
const int POS_KEY = 12;
const int PROG_KEY = 13;
const int MENU_KEY = 14;
const int PARAM_KEY = 15;
//const int ALARM_KEY = 16;
//const int AUX_KEY = 17;
const int M_KEY = 18;
const int T_KEY = 19;
const int KEY_0 = 20;
const int KEY_1 = 21;
const int KEY_2 = 35;
const int KEY_3 = 36;
const int KEY_4 = 16;
const int KEY_5 = 38;
const int KEY_6 = 17;
const int KEY_7 = 48;
const int KEY_8 = 41;
const int KEY_9 = 42;
const int BAC_KEY = 45;
const int KIH_KEY = 46;
const int JQP_KEY = 47;

std::map<String, int> buttonPins = {
  {"resetkey", RESET_KEY},
  {"cursorupkey", CURSORUP_KEY},
  {"cursordownkey", CURSORDOWN_KEY},
  {"pageupkey", PAGEUP_KEY},
  {"pagedownkey", PAGEDOWN_KEY},
  {"alterkey", ALTER_KEY},
  {"insertkey", INSERT_KEY},
  {"deletekey", DELETE_KEY},
  {"eobkey", EOB_KEY},
  {"cankey", CAN_KEY},
  {"inputkey", INPUT_KEY},
  {"outputkey", OUTPUT_KEY},
  {"poskey", POS_KEY},
  {"progkey", PROG_KEY},
  {"menukey", MENU_KEY},
  {"paramkey", PARAM_KEY},
  //{"alarmkey", ALARM_KEY},
  //{"auxkey", AUX_KEY},
  {"mkey", M_KEY},
  {"tkey", T_KEY},
  {"key0", KEY_0},
  {"key1", KEY_1},
  {"key2", KEY_2},
  {"key3", KEY_3},
  {"key4", KEY_4},
  {"key5", KEY_5},
  {"key6", KEY_6},
  {"key7", KEY_7},
  {"key8", KEY_8},
  {"key9", KEY_9},
  {"backey", BAC_KEY},
  {"kihkey", KIH_KEY},
  {"jqpkey", JQP_KEY}
};

// Function Declarations
void connectToWiFi();
void connectToMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void handleButtonPress(String buttonName);
void handleRelayControl(String buttonName);

// HTML webpage (đã sửa lại để thêm các hàm xử lý)
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
    <h1>CNC Keyboard Control</h1>
    <button class="button" onclick="sendCommand('/resetkey')">RESET</button>
    <button class="button" onclick="sendCommand('/cursorupkey')">CURSOR<br>UP</button>
    <button class="button" onclick="sendCommand('/cursordownkey')">CURSOR<br>DOWN</button>
    <button class="button" onclick="sendCommand('/pageupkey')">PAGE<br>UP</button>
    <button class="button" onclick="sendCommand('/pagedownkey')">PAGE<br>DOWN</button>
    <button class="button" onclick="sendCommand('/alterkey')">ALTER</button>
    <button class="button" onclick="sendCommand('/insertkey')">INSERT</button>
    <button class="button" onclick="sendCommand('/deletekey')">DELETE</button>
    <button class="button" onclick="sendCommand('/eobkey')">/.#<br>EOB</button>
    <button class="button" onclick="sendCommand('/cankey')">CAN</button>
    <button class="button" onclick="sendCommand('/inputkey')">INPUT</button>
    <button class="button" onclick="sendCommand('/outputkey')">OUTPUT<br>START</button>
    <button class="button" onclick="sendCommand('/poskey')">POS</button>
    <button class="button" onclick="sendCommand('/progkey')">PROGRAM</button>
    <button class="button" onclick="sendCommand('/menukey')">MENU<br>OFFSET</button>
    <button class="button" onclick="sendCommand('/paramkey')">DGNOS<br>PARAM</button>
    <button class="button" onclick="sendCommand('/alarmkey')">OPR<br>ALARM</button>
    <button class="button" onclick="sendCommand('/auxkey')">AUX<br>GRAPH</button>
    <button class="button" onclick="sendCommand('/mkey')">-<br>M</button>
    <button class="button" onclick="sendCommand('/tkey')">/<br>T</button>
    <button class="button" onclick="sendCommand('/key0')">0<br>S</button>
    <button class="button" onclick="sendCommand('/key1')">1<br>U</button>
    <button class="button" onclick="sendCommand('/key2')">2<br>Down/W</button>
    <button class="button" onclick="sendCommand('/key3')">3<br>R</button>
    <button class="button" onclick="sendCommand('/key4')">4<br>Left/X</button>
    <button class="button" onclick="sendCommand('/key5')">5<br>Sin/Z</button>
    <button class="button" onclick="sendCommand('/key6')">6<br>Right/F</button>
    <button class="button" onclick="sendCommand('/key7')">7<br>O</button>
    <button class="button" onclick="sendCommand('/key8')">8<br>Up/N</button>
    <button class="button" onclick="sendCommand('/key9')">9<br>G</button>
    <button class="button" onclick="sendCommand('/backey')">BAC<br>Back</button>
    <button class="button" onclick="sendCommand('/kihkey')">KIHY<br>Next</button>
    <button class="button" onclick="sendCommand('/jqpkey')">JQPV<br>No.</button>
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

void setup() {
  Serial.begin(115200);

  // Thiết lập chân cho tất cả các nút làm OUTPUT
  pinMode(RESET_KEY, OUTPUT);
  pinMode(CURSORUP_KEY, OUTPUT);
  pinMode(CURSORDOWN_KEY, OUTPUT);
  pinMode(PAGEUP_KEY, OUTPUT);
  pinMode(PAGEDOWN_KEY, OUTPUT);
  pinMode(ALTER_KEY, OUTPUT);
  pinMode(INSERT_KEY, OUTPUT);
  pinMode(DELETE_KEY, OUTPUT);
  pinMode(EOB_KEY, OUTPUT);
  pinMode(CAN_KEY, OUTPUT);
  pinMode(INPUT_KEY, OUTPUT);
  pinMode(OUTPUT_KEY, OUTPUT);
  pinMode(POS_KEY, OUTPUT);
  pinMode(PROG_KEY, OUTPUT);
  pinMode(MENU_KEY, OUTPUT);
  pinMode(PARAM_KEY, OUTPUT);
  //pinMode(ALARM_KEY, OUTPUT);
  //pinMode(AUX_KEY, OUTPUT);
  pinMode(M_KEY, OUTPUT);
  pinMode(T_KEY, OUTPUT);
  pinMode(KEY_0, OUTPUT);
  pinMode(KEY_1, OUTPUT);
  pinMode(KEY_2, OUTPUT);
  pinMode(KEY_3, OUTPUT);
  pinMode(KEY_4, OUTPUT);
  pinMode(KEY_5, OUTPUT);
  pinMode(KEY_6, OUTPUT);
  pinMode(KEY_7, OUTPUT);
  pinMode(KEY_8, OUTPUT);
  pinMode(KEY_9, OUTPUT);
  pinMode(BAC_KEY, OUTPUT);
  pinMode(KIH_KEY, OUTPUT);
  pinMode(JQP_KEY, OUTPUT);

  digitalWrite(RESET_KEY, LOW);
  digitalWrite(CURSORUP_KEY, LOW);
  digitalWrite(CURSORDOWN_KEY, LOW);
  digitalWrite(PAGEUP_KEY, LOW);
  digitalWrite(PAGEDOWN_KEY, LOW);
  digitalWrite(ALTER_KEY, LOW);
  digitalWrite(INSERT_KEY, LOW);
  digitalWrite(DELETE_KEY, LOW);
  digitalWrite(EOB_KEY, LOW);
  digitalWrite(CAN_KEY, LOW);
  digitalWrite(INPUT_KEY, LOW);
  digitalWrite(OUTPUT_KEY, LOW);
  digitalWrite(POS_KEY, LOW);
  digitalWrite(PROG_KEY, LOW);
  digitalWrite(MENU_KEY, LOW);
  digitalWrite(PARAM_KEY, LOW);
  //digitalWrite(ALARM_KEY, LOW);
  //digitalWrite(AUX_KEY, LOW);
  digitalWrite(M_KEY, LOW);
  digitalWrite(T_KEY, LOW);
  digitalWrite(KEY_0, LOW);
  digitalWrite(KEY_1, LOW);
  digitalWrite(KEY_2, LOW);
  digitalWrite(KEY_3, LOW);
  digitalWrite(KEY_4, LOW);
  digitalWrite(KEY_5, LOW);
  digitalWrite(KEY_6, LOW);
  digitalWrite(KEY_7, LOW);
  digitalWrite(KEY_8, LOW);
  digitalWrite(KEY_9, LOW);
  digitalWrite(BAC_KEY, LOW);
  digitalWrite(KIH_KEY, LOW);
  digitalWrite(JQP_KEY, LOW);

  connectToWiFi();

  // Khởi tạo MQTT
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  connectToMQTT();

  server.on("/", handleRoot);
  server.on("/button", HTTP_GET, handleButton);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  mqtt_client.loop();

  if (!mqtt_client.connected()) {
    connectToMQTT();
  }

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

void connectToWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void connectToMQTT() {
  while (!mqtt_client.connected()) {
    String client_id = ESP_ID "-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s...\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic); // Subscribe to the topic
      Serial.print("Subscribed to topic: ");
      Serial.println(mqtt_topic);
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" Retrying in 5 seconds.");
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = "";
  for (int i = 0; i < length; i++) {
      message += (char) payload[i];
  }
  Serial.println();

    // Check if the message is for this device
    if (String(topic) == mqtt_topic) {
        handleRelayControl(message); // Call the function to control the relay
    } else {
        Serial.print("Topic mismatch: ");
        Serial.println(topic);
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
        handleButtonPress(buttonName);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing button name");
    }
}

void handleButtonPress(String buttonName) {
  Serial.println("Button pressed: " + buttonName);
   mqtt_client.publish(mqtt_topic, buttonName.c_str());
   Serial.println("Published to MQTT : " + String(buttonName));

}
void handleRelayControl(String buttonName) {
    Serial.println("Handle Relay control: "+buttonName);
  if (buttonPins.count(buttonName)) {
      int keyPin = buttonPins[buttonName];
      digitalWrite(keyPin, HIGH);
      delay(200);
      digitalWrite(keyPin, LOW);
      Serial.print("Sent HIGH to button: ");
      Serial.println(buttonName);
  } else {
    Serial.println("Unknown command: " + buttonName);
  }
}
