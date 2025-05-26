const int plPin = 5;     // PL (chân 1)
const int clkPin = 4;   // CLK (chân 2)
const int dataPin = 15;  // QH (chân 9)

void setup() {
  Serial.begin(115200);
  pinMode(plPin, OUTPUT);
  pinMode(clkPin, OUTPUT);
  pinMode(dataPin, INPUT);

  digitalWrite(clkPin, LOW);
  digitalWrite(plPin, HIGH);
}

void loop() {
  // Chốt dữ liệu
  digitalWrite(plPin, LOW);
  delayMicroseconds(5);
  digitalWrite(plPin, HIGH);

  // Đọc 8 bit từ 74HC165
  byte inputBits = shiftIn(dataPin, clkPin, MSBFIRST);

  // Chỉ kiểm tra D0, D1, D2
  bool d0 = bitRead(inputBits, 1);  // D1
  bool d1 = bitRead(inputBits, 2);  // D2
  bool d2 = bitRead(inputBits, 3);  // D3
  bool d3 = bitRead(inputBits, 4);  // D4
  bool d4 = bitRead(inputBits, 5);  // D5
  bool d5 = bitRead(inputBits, 6);  // D6
  bool d6 = bitRead(inputBits, 7);  // D7
  bool d7 = bitRead(inputBits, 0);  // D8
  
  Serial.print("D0: "); Serial.println(d0 ? "ON" : "OFF");
  Serial.print("D1: "); Serial.println(d1 ? "ON" : "OFF");
  Serial.print("D2: "); Serial.println(d2 ? "ON" : "OFF");
  Serial.print("D3: "); Serial.println(d3 ? "ON" : "OFF");
  Serial.print("D4: "); Serial.println(d4 ? "ON" : "OFF");
  Serial.print("D5: "); Serial.println(d5 ? "ON" : "OFF");
  Serial.print("D6: "); Serial.println(d6 ? "ON" : "OFF");
  Serial.print("D7: "); Serial.println(d7 ? "ON" : "OFF");

  Serial.println();
  delay(300);
}
