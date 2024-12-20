#include <WiFi.h>
#include <WebServer.h>

// Thay đổi thông tin WiFi của bạn tại đây
const char* ssid = "PHUC";
const char* password = "04122002";

WebServer server(80);
const int LED_PIN = 4;
bool ledState = false;

// HTML webpage
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>LED Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 20px;
        }
        .button {
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 15px 32px;
            text-align: center;
            display: inline-block;
            font-size: 16px;
            margin: 4px 2px;
            cursor: pointer;
            border-radius: 4px;
        }
        .button-off {
            background-color: #f44336;
        }
    </style>
</head>
<body>
    <h1>ESP8266 LED Control</h1>
    <p>LED Status: <span id="ledStatus">OFF</span></p>
    <button class="button" onclick="toggleLED()">Toggle LED</button>
    
    <script>
        function toggleLED() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (this.readyState == 4 && this.status == 200) {
                    document.getElementById("ledStatus").innerHTML = this.responseText;
                }
            };
            xhr.open("GET", "/toggle", true);
            xhr.send();
        }
    </script>
</body>
</html>
)=====";

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
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
    
    // Định nghĩa các endpoint
    server.on("/", handleRoot);
    server.on("/toggle", handleToggle);
    
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

void handleToggle() {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    server.send(200, "text/plain", ledState ? "ON" : "OFF");
}