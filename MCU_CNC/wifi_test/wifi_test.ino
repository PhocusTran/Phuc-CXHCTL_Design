#include <WiFi.h>
#include <WebServer.h>

#ifdef USB_DEBUG
#include <Arduino.h>

#define Debug_Message(...)   Serial.printf(__VA_ARGS__);Serial.println();Serial.flush()
#define Debug_Putc(c)        Serial.write(c)
#else
#define Debug_Message(...)
#define Debug_Putc(c)
#endif

// Thông tin Wi-Fi
const char* ssid = "PHUC"; // Thay đổi thành SSID của bạn
const char* password = "04122002"; // Thay đổi thành mật khẩu của bạn

WebServer server(80); // Khởi tạo web server trên cổng 80

void setup() {
  Serial.begin(115200);

  // Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Đang kết nối...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Đã kết nối Wi-Fi, địa chỉ IP: ");
  Serial.println(WiFi.localIP());

  // Xử lý request đến trang chủ "/"
  server.on("/", []() {
    server.send(200, "text/html", "<h1>Hello World!</h1>");
  });

  server.begin(); // Bắt đầu web server
  Serial.println("Web server đã khởi động");
}

void loop() {
  server.handleClient(); // Xử lý các request từ client
}
