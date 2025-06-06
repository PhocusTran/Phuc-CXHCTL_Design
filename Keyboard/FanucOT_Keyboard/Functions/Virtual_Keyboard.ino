#include <WiFi.h>
#include <WebServer.h>
#include <driver/gpio.h>

// Thông tin WiFi
const char* ssid = "HCTL_DEMO";
const char* password = "123456789HCTL";

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
const int ALARM_KEY = 16;
const int AUX_KEY = 17;
const int M_KEY = 18;
const int T_KEY = 19;
const int KEY_0 = 20;
const int KEY_1 = 21;
const int KEY_2 = 35;
const int KEY_3 = 36;
const int KEY_4 = 37;
const int KEY_5 = 38;
const gpio_num_t KEY_6 = GPIO_NUM_43; 
const int KEY_7 = 48;
const int KEY_8 = 41;
const int KEY_9 = 42;
const int BAC_KEY = 45;
const int KIH_KEY = 46;
const int JQP_KEY = 47;


bool resetkeyState = false;
bool cursorupkeyState = false;
bool cursordownkeyState = false;
bool pageupkeyState = false;
bool pagedownkeyState = false;
bool alterkeyState = false;
bool insertkeyState = false;
bool deletekeyState = false;
bool eobkeyState = false;
bool cankeyState = false;
bool inputkeyState = false;
bool outputkeyState = false;
bool poskeyState = false;
bool progkeyState = false;
bool menukeyState = false;
bool paramkeyState = false;
bool alarmkeyState = false;
bool auxkeyState = false;
bool mkeyState = false;
bool tkeyState = false;
bool key0State = false;
bool key1State = false;
bool key2State = false;
bool key3State = false;
bool key4State = false;
bool key5State = false;
bool key6State = false;
bool key7State = false;
bool key8State = false;
bool key9State = false;
bool backeyState = false;
bool kihkeyState = false;
bool jqpkeyState = false;

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
            xhr.open("GET", command, true);
            xhr.send();
        }
    </script>
</body>
</html>
)=====";

// Hàm setup cho từng chân GPIO
void setupGPIO(gpio_num_t gpioNum) {
  gpio_reset_pin(gpioNum);
  gpio_set_direction(gpioNum, GPIO_MODE_OUTPUT);
  gpio_set_level(gpioNum, 0);
}

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
  pinMode(ALARM_KEY, OUTPUT);
  pinMode(AUX_KEY, OUTPUT);
  pinMode(M_KEY, OUTPUT);
  pinMode(T_KEY, OUTPUT);
  pinMode(KEY_0, OUTPUT);
  pinMode(KEY_1, OUTPUT);
  pinMode(KEY_2, OUTPUT);
  pinMode(KEY_3, OUTPUT);
  pinMode(KEY_4, OUTPUT);
  pinMode(KEY_5, OUTPUT);
  setupGPIO(KEY_6);
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
  digitalWrite(ALARM_KEY, LOW);
  digitalWrite(AUX_KEY, LOW);
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
  
  
  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.println("\nĐang kết nối WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  
  Serial.println("\nĐã kết nối WiFi");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());

  // Định nghĩa các endpoint cho tất cả các nút
  server.on("/", handleRoot);
  server.on("/resetkey", handleResetKey);
  server.on("/cursorupkey", handleCursorUpKey);
  server.on("/cursordownkey", handleCursorDownKey);
  server.on("/pageupkey", handlePageUpKey);
  server.on("/pagedownkey", handlePageDownKey);
  server.on("/alterkey", handleAlterKey);
  server.on("/insertkey", handleInsertKey);
  server.on("/deletekey", handleDeleteKey);
  server.on("/eobkey", handleEobKey);
  server.on("/cankey", handleCanKey);
  server.on("/inputkey", handleInputKey);
  server.on("/outputkey", handleOutputKey);
  server.on("/poskey", handlePosKey);
  server.on("/progkey", handleProgKey);
  server.on("/menukey", handleMenuKey);
  server.on("/paramkey", handleParamKey);
  server.on("/alarmkey", handleAlarmKey);
  server.on("/auxkey", handleAuxKey);
  server.on("/mkey", handleMKey);
  server.on("/tkey", handleTKey);
  server.on("/key0", handleKey0);
  server.on("/key1", handleKey1);
  server.on("/key2", handleKey2);
  server.on("/key3", handleKey3);
  server.on("/key4", handleKey4);
  server.on("/key5", handleKey5);
  server.on("/key6", handleKey6);
  server.on("/key7", handleKey7);
  server.on("/key8", handleKey8);
  server.on("/key9", handleKey9);
  server.on("/backey", handleBacKey);
  server.on("/kihkey", handleKihKey);
  server.on("/jqpkey", handleJqpKey);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  String html = MAIN_page;
  server.send(200, "text/html", html);
}

// Các hàm xử lý cho từng nút (ví dụ)
void handleResetKey() { sendSignal(RESET_KEY); }
void handleCursorUpKey() { sendSignal(CURSORUP_KEY); }
void handleCursorDownKey() { sendSignal(CURSORDOWN_KEY); }
void handlePageUpKey() { sendSignal(PAGEUP_KEY); }
void handlePageDownKey() { sendSignal(PAGEDOWN_KEY); }
void handleAlterKey() { sendSignal(ALTER_KEY); }
void handleInsertKey() { sendSignal(INSERT_KEY); }
void handleDeleteKey() { sendSignal(DELETE_KEY); }
void handleEobKey() { sendSignal(EOB_KEY); }
void handleCanKey() { sendSignal(CAN_KEY); }
void handleInputKey() { sendSignal(INPUT_KEY); }
void handleOutputKey() { sendSignal(OUTPUT_KEY); }
void handlePosKey() { sendSignal(POS_KEY); }
void handleProgKey() { sendSignal(PROG_KEY); }
void handleMenuKey() { sendSignal(MENU_KEY); }
void handleParamKey() { sendSignal(PARAM_KEY); }
void handleAlarmKey() { sendSignal(ALARM_KEY); }
void handleAuxKey() { sendSignal(AUX_KEY); }
void handleMKey() { sendSignal(M_KEY); }
void handleTKey() { sendSignal(T_KEY); }
void handleKey0() { sendSignal(KEY_0); }
void handleKey1() { sendSignal(KEY_1); }
void handleKey2() { sendSignal(KEY_2); }
void handleKey3() { sendSignal(KEY_3); }
void handleKey4() { sendSignal(KEY_4); }
void handleKey5() { sendSignal(KEY_5); }
void handleKey6() { sendSignal(KEY_6); }
void handleKey7() { sendSignal(KEY_7); }
void handleKey8() { sendSignal(KEY_8); }
void handleKey9() { sendSignal(KEY_9); }
void handleBacKey() { sendSignal(BAC_KEY); }
void handleKihKey() { sendSignal(KIH_KEY); }
void handleJqpKey() { sendSignal(JQP_KEY); }

void sendSignal(int keyPin) {
  digitalWrite(keyPin, HIGH);
  server.send(200, "text/plain", "OK");
  delay(1000);
  digitalWrite(keyPin, LOW);
}
