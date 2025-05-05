#include <Arduino.h>

const int F5_KEY = 7;
const int F6_KEY = 8;
const int F7_KEY = 9;

void setup() {
    Serial.begin(115200);
    pinMode(F5_KEY, OUTPUT);
    pinMode(F6_KEY, OUTPUT);
    pinMode(F7_KEY, OUTPUT);

    Serial.println("Test started");
}
 
void loop() {
    Serial.println("F5 HIGH");
    digitalWrite(F5_KEY, HIGH);
    delay(200);
    Serial.println("F5 LOW");
    digitalWrite(F5_KEY, LOW);

    Serial.println("F6 HIGH");
    digitalWrite(F6_KEY, HIGH);
    delay(200);
    Serial.println("F6 LOW");
    digitalWrite(F6_KEY, LOW);

    Serial.println("F7 HIGH");
    digitalWrite(F7_KEY, HIGH);
    delay(200);
    Serial.println("F7 LOW");
    digitalWrite(F7_KEY, LOW);

    delay(2000); // Chờ 2 giây trước khi lặp lại
}
