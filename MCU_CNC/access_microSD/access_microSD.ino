#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_TinyUSB.h>

// === Configuration ===
#define SD_CS 10  // Chip Select pin cho thẻ microSD

// === Globals ===
SdFat sd;                // Hệ thống file SD
Adafruit_USBD_MSC usb_msc; // Đối tượng USB Mass Storage
uint32_t block_count = 0;  // Dung lượng SD card (tính theo sector 512-byte)

// === Function Prototypes ===
bool sdCardInit();
bool usbMscInit();

// === Callbacks ===
// Hàm đọc dữ liệu từ SD khi có yêu cầu từ USB
int32_t msc_read_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
  Serial.print("[MSC READ] LBA = "); Serial.print(lba);
  Serial.print(", BufSize = "); Serial.println(bufsize);

  if (!sd.card()->readSectors(lba, (uint8_t*)buffer, bufsize / 512)) {
    Serial.println("[ERROR] Read failed!");
    return -1; // Lỗi
  }

  Serial.println("[SUCCESS] Read complete.");
  return bufsize; // OK
}

// Hàm ghi dữ liệu từ USB vào SD
int32_t msc_write_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  Serial.print("[MSC WRITE] LBA = "); Serial.print(lba);
  Serial.print(", BufSize = "); Serial.println(bufsize);

  if (!sd.card()->writeSectors(lba, buffer, bufsize / 512)) {
    Serial.println("[ERROR] Write failed!");
    return -1; // Lỗi
  }

  Serial.println("[SUCCESS] Write complete.");
  return bufsize; // OK
}

// Hàm đồng bộ dữ liệu
void msc_flush_cb(void) {
  Serial.println("[MSC FLUSH] Syncing data...");
  sd.card()->syncDevice();
  Serial.println("[SUCCESS] Data synced.");
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  while (!Serial); // Chờ Serial kết nối
  Serial.println("\n[INFO] ESP32-S3 USB MSC with SD Card");

  if (!sdCardInit()) {
    Serial.println("[FATAL ERROR] SD Card Initialization Failed!");
    while (1); // Dừng chương trình
  }

  if (!usbMscInit()) {
    Serial.println("[FATAL ERROR] USB MSC Initialization Failed!");
    while (1); // Dừng chương trình
  }

  Serial.println("[INFO] Initialization Complete. Connect USB to your computer.");
}

// === Loop ===
void loop() {
  delay(1000); // Không cần làm gì thêm
}

// === SD Card Initialization ===
bool sdCardInit() {
  Serial.print("[INFO] Initializing SD card...");
  if (!sd.begin(SD_CS, SD_SCK_MHZ(10))) {
    Serial.println("[ERROR] SD card initialization failed!");
    sd.initErrorPrint(&Serial);  // In chi tiết lỗi
    return false;
  }
  Serial.println("[SUCCESS] SD card initialized.");

  Serial.print("[INFO] Getting block count...");
  block_count = sd.card()->sectorCount();
  Serial.print("Block count: "); Serial.println(block_count);

  Serial.print("[INFO] Card size: ");
  Serial.print((float)block_count / 2 / 1024, 2);
  Serial.println(" MB");

  return true;
}

// === USB MSC Initialization ===
bool usbMscInit() {
  Serial.println("[INFO] Initializing USB MSC");

  // Thiết lập ID của thiết bị USB
  usb_msc.setID("ESP32-S3", "USB Mass Storage", "1.0");
  Serial.println("[SUCCESS] USB ID set.");

  // Đăng ký callback cho MSC
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);
  Serial.println("[SUCCESS] Callbacks set.");

  // Xác định kích thước ổ đĩa
  usb_msc.setCapacity(block_count, 512);
  Serial.println("[SUCCESS] Capacity set.");

  // Báo hiệu thiết bị sẵn sàng
  usb_msc.setUnitReady(true);
  Serial.println("[SUCCESS] USB Mass Storage ready.");

  // Khởi động USB MSC
  usb_msc.begin();
  Serial.println("[SUCCESS] USB MSC initialized.");
  
  return true;
}
