int ledPins[] = {0, 1, 2, 3, 4, 5, 6, 7}; // Sử dụng các chân GPIO có thể dùng làm output trên ESP32
int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);

void setup() {
  // Khởi tạo các chân LED là OUTPUT
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
}

void loop() {
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);   // Bật LED hiện tại
    delay(300);                     // Đợi 300ms
    digitalWrite(ledPins[i], LOW);    // Tắt LED hiện tại
  }
}
