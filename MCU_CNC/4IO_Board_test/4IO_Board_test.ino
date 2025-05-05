#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "PHUC"; 
const char* password = "04122002"; 

#define BUTTON_Y0 0
#define BUTTON_Y1 2
#define BUTTON_Y2 4
#define BUTTON_Y3 15

bool buttonStateY0 = false;
bool buttonStateY1 = false;
bool buttonStateY2 = false;
bool buttonStateY3 = false;

WebServer server(80);

void handleRoot() {
  String html = "<!DOCTYPE html>\
                 <html>\
                 <head>\
                   <title>ESP32 Button Control</title>\
                   <style>\
                     .button { padding: 10px 20px; font-size: 20px; }\
                   </style>\
                 </head>\
                 <body>\
                   <h1>ESP32 Button Control</h1>\
                   <p>Button Y0: <a href='/buttonY0' class='button'>Toggle</a> (" + String(buttonStateY0 ? "ON" : "OFF") + ")</p>\
                   <p>Button Y1: <a href='/buttonY1' class='button'>Toggle</a> (" + String(buttonStateY1 ? "ON" : "OFF") + ")</p>\
                   <p>Button Y2: <a href='/buttonY2' class='button'>Toggle</a> (" + String(buttonStateY2 ? "ON" : "OFF") + ")</p>\
                   <p>Button Y3: <a href='/buttonY3' class='button'>Toggle</a> (" + String(buttonStateY3 ? "ON" : "OFF") + ")</p>\
                 </body>\
                 </html>";
  server.send(200, "text/html", html);
}

void handleButtonY0() {
  buttonStateY0 = !buttonStateY0;
  digitalWrite(BUTTON_Y0, buttonStateY0 ? HIGH : LOW);
  handleRoot(); // Quay lại trang chính
}

void handleButtonY1() {
  buttonStateY1 = !buttonStateY1;
  digitalWrite(BUTTON_Y1, buttonStateY1 ? HIGH : LOW);
  handleRoot(); // Quay lại trang chính
}

void handleButtonY2() {
  buttonStateY2 = !buttonStateY2;
  digitalWrite(BUTTON_Y2, buttonStateY2 ? HIGH : LOW);
  handleRoot(); // Quay lại trang chính
}

void handleButtonY3() {
  buttonStateY3 = !buttonStateY3;
  digitalWrite(BUTTON_Y3, buttonStateY3 ? HIGH : LOW);
  handleRoot(); // Quay lại trang chính
}

void setup() {
  Serial.begin(115200);

  // Cấu hình các chân GPIO làm OUTPUT
  pinMode(BUTTON_Y0, OUTPUT);
  pinMode(BUTTON_Y1, OUTPUT);
  pinMode(BUTTON_Y2, OUTPUT);
  pinMode(BUTTON_Y3, OUTPUT);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected to WiFi, IP address: ");
  Serial.println(WiFi.localIP());

  // Định nghĩa các đường dẫn
  server.on("/", handleRoot);
  server.on("/buttonY0", handleButtonY0);
  server.on("/buttonY1", handleButtonY1);
  server.on("/buttonY2", handleButtonY2);
  server.on("/buttonY3", handleButtonY3);

  // Bắt đầu server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
