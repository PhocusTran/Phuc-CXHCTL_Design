#include <WiFi.h>
#include <WebServer.h>

// Thông tin WiFi
const char* ssid = "PHUC";
const char* password = "04122002";

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
const int KEY_6 = 39;
const int KEY_7 = 40;
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
<head>
<title>CNC Keyboard Control</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {
    font-family: Arial, sans-serif;
    margin: 20px;
}
.button {
    width: 100px;
    height: 60px;
    padding: 10px;
    line-height: 1.2;
    border-radius: 4px;
    border: none;
    cursor: pointer;
    box-sizing: border-box;
}

.button-red { background-color: red; color: white; }
.button-green { background-color: #4CAF50; color: white; }
.button-yellow { background-color: yellow; color: black; }

.container {
    display: grid;
    grid-template-columns: repeat(7, 1fr); /* 7 cột */
    grid-gap: 10px; /* Khoảng cách giữa các phần tử */
}

/* Định nghĩa vị trí của các nút bằng JavaScript */
</style>
</head>
<body>
<h1>CNC Keyboard Control</h1>
<div class="container" id="buttonContainer"></div>

<script>
const buttonData = [
    {text: 'RESET', class: 'button-red', col: 1, row: 0},
    {text: 'CURSOR\nUP', class: 'button-green', col: 1, row: 2},
    {text: 'CURSOR\nDOWN', class: 'button-green', col: 1, row: 3},
    {text: 'PAGE\nUP', class: 'button-green', col: 1, row: 5},
    {text: 'PAGE\nDOWN', class: 'button-green', col: 1, row: 6},
    {text: '7\nO', class: 'button-yellow', col: 2, row: 0},
    {text: '8\nUp/N', class: 'button-yellow', col: 3, row: 0},
    {text: '9\nG', class: 'button-yellow', col: 4, row: 0},
    {text: '4\nLeft/X', class: 'button-yellow', col: 2, row: 1},
    {text: '5\nSin/Z', class: 'button-yellow', col: 3, row: 1},
    {text: '6\nRight/F', class: 'button-yellow', col: 4, row: 1},
    {text: '1\nU', class: 'button-yellow', col: 2, row: 2},
    {text: '2\nDown/W', class: 'button-yellow', col: 3, row: 2},
    {text: '3\nR', class: 'button-yellow', col: 4, row: 2},
    {text: '-\nM', class: 'button-yellow', col: 2, row: 3},
    {text: '0\nS', class: 'button-yellow', col: 3, row: 3},
    {text: '/\nT', class: 'button-yellow', col: 4, row: 3},
    {text: 'BAC\nBack', class: 'button-yellow', col: 2, row: 4},
    {text: 'KIHY\nNext', class: 'button-yellow', col: 3, row: 4},
    {text: 'JQPV\nNo.', class: 'button-yellow', col: 4, row: 4},
    {text: 'POS', class: 'button-green', col: 2, row: 5},
    {text: 'PROGRAM', class: 'button-green', col: 3, row: 5},
    {text: 'MENU\nOFFSET', class: 'button-green', col: 4, row: 5},
    {text: 'DGNOS\nPARAM', class: 'button-green', col: 2, row: 6},
    {text: 'OPR\nALARM', class: 'button-green', col: 3, row: 6},
    {text: 'AUX\nGRAPH', class: 'button-green', col: 4, row: 6},
    {text: 'ALTER', class: 'button-green', col: 5, row: 0},
    {text: 'INSERT', class: 'button-green', col: 5, row: 1},
    {text: 'DELETE', class: 'button-green', col: 5, row: 2},
    {text: '/.#\nEOB', class: 'button-yellow', col: 5, row: 3},
    {text: 'CAN', class: 'button-green', col: 5, row: 4},
    {text: 'INPUT', class: 'button-green', col: 5, row: 5},
    {text: 'OUTPUT\nSTART', class: 'button-green', col: 5, row: 6}
];

const buttonContainer = document.getElementById('buttonContainer');

buttonData.forEach(button => {
    const buttonElement = document.createElement('button');
    buttonElement.textContent = button.text;
    buttonElement.className = `button ${button.class}`;
    buttonElement.style.gridColumn = button.col + 1; // index bắt đầu từ 1
    buttonElement.style.gridRow = button.row + 1;    // index bắt đầu từ 1
    buttonElement.addEventListener('click', () => {
        sendCommand('/' + button.text.toLowerCase().replace(/\s+/g, '').replace('\n', ''));
    });
    buttonContainer.appendChild(buttonElement);
});


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
  pinMode(KEY_6, OUTPUT);
  pinMode(KEY_7, OUTPUT);
  pinMode(KEY_8, OUTPUT);
  pinMode(KEY_9, OUTPUT);
  pinMode(BAC_KEY, OUTPUT);
  pinMode(KIH_KEY, OUTPUT);
  pinMode(JQP_KEY, OUTPUT);

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

  // Chỉ cần một hàm handleButtonPress duy nhất
  server.on("/(.*)", handleButtonPress); // Xử lý tất cả các request

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

void handleButtonPress() {
  String command = server.uri();
  command.remove(0, 1); // Loại bỏ dấu '/' ở đầu

  Serial.print("Button pressed: ");
  Serial.println(command);

  // Sử dụng switch-case để xử lý các nút
  int pin = -1;
  if (command == "resetkey") pin = RESET_KEY;
  else if (command == "cursorupkey") pin = CURSORUP_KEY;
  else if (command == "cursordownkey") pin = CURSORDOWN_KEY;
  else if (command == "pageupkey") pin = PAGEUP_KEY;
  else if (command == "pagedownkey") pin = PAGEDOWN_KEY;
  else if (command == "alterkey") pin = ALTER_KEY;
  else if (command == "insertkey") pin = INSERT_KEY;
  else if (command == "deletekey") pin = DELETE_KEY;
  else if (command == "eobkey") pin = EOB_KEY;
  else if (command == "cankey") pin = CAN_KEY;
  else if (command == "inputkey") pin = INPUT_KEY;
  else if (command == "outputkey") pin = OUTPUT_KEY;
  else if (command == "poskey") pin = POS_KEY;
  else if (command == "progkey") pin = PROG_KEY;
  else if (command == "menukey") pin = MENU_KEY;
  else if (command == "paramkey") pin = PARAM_KEY;
  else if (command == "alarmkey") pin = ALARM_KEY;
  else if (command == "auxkey") pin = AUX_KEY;
  else if (command == "mkey") pin = M_KEY;
  else if (command == "tkey") pin = T_KEY;
  else if (command == "key0") pin = KEY_0;
  else if (command == "key1") pin = KEY_1;
  else if (command == "key2") pin = KEY_2;
  else if (command == "key3") pin = KEY_3;
  else if (command == "key4") pin = KEY_4;
  else if (command == "key5") pin = KEY_5;
  else if (command == "key6") pin = KEY_6;
  else if (command == "key7") pin = KEY_7;
  else if (command == "key8") pin = KEY_8;
  else if (command == "key9") pin = KEY_9;
  else if (command == "backey") pin = BAC_KEY;
  else if (command == "kihkey") pin = KIH_KEY;
  else if (command == "jqpkey") pin = JQP_KEY;

  if (pin != -1) {
    digitalWrite(pin, HIGH);
    delay(50);
    digitalWrite(pin, LOW);
  } else {
    Serial.println("Unknown command");
  }

  server.send(200, "text/plain", "OK");
}
