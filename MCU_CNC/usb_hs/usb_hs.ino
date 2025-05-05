#include "USB.h"
#include "USBMSC.h"

void setup() {
  Serial.begin(115200);
  delay(2000);

  USB.setBCDUSB(0x0200);   // USB 2.0 High-Speed
  USB.setMaxPacketSize0(64); // 64 bytes

  Serial.println("USB Configuration:");
  Serial.print("  bcdUSB: 0x");
  Serial.println(USB.getBCDUSB(), HEX); // Giả sử có hàm getBCDUSB()
  Serial.print("  bMaxPacketSize0: ");
  Serial.println(USB.getMaxPacketSize0()); // Giả sử có hàm getMaxPacketSize0()

  // ... Khởi tạo USB, MSC, v.v.
}
