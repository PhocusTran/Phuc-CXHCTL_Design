#ifndef ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBMSC.h"
#include "SPI.h"
#include "SdFat.h"
#include "Timer.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FreeRTOS.h>
#include <semphr.h>

// Định nghĩa các chân SPI
#define SPI_CS                  10
#define SPI_MOSI                11
#define SPI_MISO                13
#define SPI_SCK                 12

// Khởi tạo SdFat
SdFat sd;
SdFile myFile;
Timer timer;

// Cấu hình Serial
#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
USBCDC USBSerial;
#endif

// Khởi tạo USB MSC
USBMSC MSC;

// Khởi tạo Web Server
AsyncWebServer server(80);

// Thông tin WiFi
const char* ssid = "PHUC";
const char* password = "04122002";

// Biến toàn cục cho copy/paste
String copiedFilename = "";
String copiedData = "";

// Định nghĩa sector size
static const uint16_t DISK_SECTOR_SIZE = 512;
static uint32_t sectors = 0;
static uint32_t readCounter, writeCounter, busyCounter = 0;

// Mutex cho việc truy cập SD card
SemaphoreHandle_t sdMutex;

// Biến lưu trữ thông tin dung lượng
uint64_t cardSize;
uint64_t freeSpace;
uint64_t usedSpace;

// Forward declarations
void startWebServer();
void printReadWriteCounter(void);
bool sdReadSectorMutex(uint32_t lba, void* buffer);
bool sdWriteSectorMutex(uint32_t lba, const void* buffer);

// Hàm đọc sector với mutex
bool sdReadSectorMutex(uint32_t lba, void* buffer) {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    bool result = sd.card()->readSector(lba, (uint8_t*)buffer); // Ép kiểu buffer
    xSemaphoreGive(sdMutex);
    return result;
  } else {
    Serial.println("Failed to acquire mutex for read!");
    return false;
  }
}

// Hàm ghi sector với mutex
bool sdWriteSectorMutex(uint32_t lba, const void* buffer) {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
    bool result = sd.card()->writeSector(lba, (const uint8_t*)buffer);
    xSemaphoreGive(sdMutex);
    return result;
  } else {
    Serial.println("Failed to acquire mutex for write!");
    return false;
  }
}

// Callback cho việc ghi từ USB MSC
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize){
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  {
    writeCounter += bufsize/DISK_SECTOR_SIZE;
    if (sdWriteSectorMutex(lba, buffer)) {
      return bufsize;
    } else {
      return -1;
    }
  }
  else
  {
    writeCounter++;
    if (sdWriteSectorMutex(lba, buffer)) {
      return bufsize;
    } else {
      return -1;
    }
  }
}

// Callback cho việc đọc từ USB MSC
static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize){
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  {
    readCounter += bufsize/DISK_SECTOR_SIZE;
    if (sdReadSectorMutex(lba, buffer)) {
      return bufsize;
    } else {
      return -1;
    }
  }
  else
  {
    readCounter++;
    if (sdReadSectorMutex(lba, buffer)) {
      return bufsize;
    } else {
      return -1;
    }
  }
}

// Callback cho việc flush từ USB MSC
void sdcard_flush_cb (void)
{
  while(sd.card()->isBusy());
  bool syncSuccess = sd.card()->syncDevice(); // Lưu kết quả sync
  if (!syncSuccess) {
      Serial.println("SD Card syncDevice() failed!");
  }
}

// Callback cho việc start/stop USB MSC
static bool onStartStop(uint8_t power_condition, bool start, bool load_eject){
  HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
  return true;
}

// Callback cho các sự kiện USB
static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base == ARDUINO_USB_EVENTS){
    arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_STARTED_EVENT:
        HWSerial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        HWSerial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        HWSerial.println("USB RESUMED");
        break;
      default:
        break;
    }
  }
}

void setup() {
  HWSerial.begin(115200);
  HWSerial.setDebugOutput(true);

  // Khởi tạo mutex
  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    HWSerial.println("Failed to create mutex!");
    while(1);
  }

  // Khởi tạo SD
  HWSerial.print("SD init ... ");
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SdSpiConfig spiConfig(SPI_CS, SHARED_SPI, SD_SCK_MHZ(10));
  if (!sd.begin(spiConfig)) {
      HWSerial.println("sd.begin() failed ...");
      while(1);
  }
  HWSerial.println("done");

  cardSize = sd.card()->cardSize() * 512; // Tổng dung lượng (bytes)
  freeSpace = sd.vol()->freeClusterCount() * sd.vol()->sectorsPerCluster() * 512; // Dung lượng trống (bytes)
  usedSpace = cardSize - freeSpace;

  HWSerial.print("Card Size: ");
  HWSerial.print(cardSize);
  HWSerial.println(" bytes");
  HWSerial.print("Free Space: ");
  HWSerial.print(freeSpace);
  HWSerial.println(" bytes");
  HWSerial.print("Used Space: ");
  HWSerial.print(usedSpace);
  HWSerial.println(" bytes");

  sectors = sd.card()->sectorCount();
  HWSerial.print("Sectors: ");
  HWSerial.println(sectors);

  // Khởi tạo USB MSC
  USB.onEvent(usbEventCallback);
  MSC.vendorID("USBMSC");
  MSC.productID("USBMSC");
  MSC.productRevision("1.0");
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.mediaPresent(true);
  MSC.begin(sectors, DISK_SECTOR_SIZE);
  USBSerial.begin();
  USB.begin();

  // Khởi tạo WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Khởi động web server
  startWebServer();

  // Khởi tạo timer
  timer.setInterval(1000);
  timer.setCallback(printReadWriteCounter);
  timer.start();
}

void printReadWriteCounter( void )
{
  HWSerial.printf("ReadCounter: %d WriteCounter: %d, BusyCounter: %d\r\n", readCounter, writeCounter, busyCounter);
}

void loop() {
  timer.update();
}

void startWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<h1>ESP32 SD Card Web Interface</h1>";
    html += "<p>Used / Total: " + String(usedSpace / 1024.0 / 1024.0, 2) + " MB / " + String(cardSize / 1024.0 / 1024.0, 2) + " MB</p>";
    html += "<button onclick='loadFileList()'>Refresh File List</button><br>";
    html += "<div id='fileListContainer'>";
    html += "</div>"; // Nơi hiển thị danh sách file

    html += "<form action='/upload' method='POST' enctype='multipart/form-data'>";
    html += "  Upload File: <input type='file' name='file'><br>";
    html += "  <input type='submit' value='Upload'>";
    html += "</form>";
    html += "<br>";
    html += "<form action='/new' method='GET'>";
    html += "  New: <select name='type'>";
    html += "    <option value='file'>File</option>";
    html += "    <option value='folder'>Folder</option>";
    html += "  </select>";
    html += "  Name: <input type='text' name='name'><br>";
    html += "  <input type='submit' value='Create'>";
    html += "</form>";

    html += "<script>";
    html += "function loadFileList() {";
    html += "  var xhttp = new XMLHttpRequest();";
    html += "  xhttp.onreadystatechange = function() {";
    html += "    if (this.readyState == 4 && this.status == 200) {";
    html += "      document.getElementById('fileListContainer').innerHTML = this.responseText;";
    html += "    }";
    html += "  };";
    html += "  xhttp.open('GET', '/list', true);";
    html += "  xhttp.send();";
    html += "}";
    html += "</script>";

    request->send(200, "text/html", html);
  });

  // List files route - chỉ trả về HTML cho danh sách file
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String fileList = "<p>Used / Total: " + String(usedSpace / 1024.0 / 1024.0, 2) + " MB / " + String(cardSize / 1024.0 / 1024.0, 2) + " MB</p>";
    fileList += "<ul>";

    SdFile root; // Sử dụng SdFile
    if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
        root.open("/");
        if (!root.isOpen()) {
          fileList += "<li>Failed to open root directory</li>";
        } else {
          SdFile entry;
          char filenameBuffer[32];
          while (entry.openNext(&root, O_RDONLY)) { //Sửa
            entry.getName(filenameBuffer, sizeof(filenameBuffer));
            String filename = String(filenameBuffer);

            if (entry.isDir()) { //Sửa
              fileList += "<li><b>" + filename + "/</b></li>";
            } else {
              fileList += "<li>"
                          "<a href='/download?file=" + filename + "'>" + filename + "</a> (" + entry.fileSize() + " bytes) "
                          "<a href='/copy_select?file=" + filename + "'>Copy</a> "
                          "<a href='/delete?file=" + filename + "'>Delete</a>"
                          "</li>";
            }
            entry.close();  // Đóng file sau khi dùng
          }
          root.close();  // Đóng file sau khi dùng

          //Thêm nút Paste nếu đã có file được copy
          if (copiedFilename != "") {
            fileList += "<br><a href='/paste_destination'>Paste Here</a> (Copied: " + copiedFilename + ")";
          }

        }
        xSemaphoreGive(sdMutex);
    } else {
        fileList += "<li>Failed to acquire mutex for file list!</li>";
    }
    fileList += "</ul>";
    request->send(200, "text/html", fileList);
  });

  // Upload file route
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(302, "text/html", "File uploaded successfully! <a href='/list'>Back</a>.  Please eject and re-plug the USB drive to see the changes in File Explorer.");
    response->addHeader("Location", "/list");
    request->send(response);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    static SdFile myFile;
    if (!index) {
      Serial.println("Upload Start: " + String(filename));
      myFile.close(); // Đảm bảo file trước đó đã đóng (nếu có)
      if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE){
          if(!myFile.open(("/" + filename).c_str(), O_WRONLY | O_CREAT | O_TRUNC)){  // Sử dụng c_str() và đường dẫn đầy đủ
            Serial.println("Error opening file for writing");
            request->send(500, "text/plain", "Failed to open file for writing");
            xSemaphoreGive(sdMutex);
            return;
          }
      } else {
        Serial.println("Failed to acquire mutex for upload!");
        request->send(500, "text/plain", "Mutex acquisition failed");
        return;
      }
    }
    Serial.printf("Upload Data: %s, %u bytes\n", filename.c_str(), (unsigned int)len);
    if (myFile.write(data, len) != len) {
      Serial.println("Write failed");
    }
    if (final) {
      Serial.println("Upload End: " + String(filename) + " (" + index + len + " bytes)");
      myFile.close();
      xSemaphoreGive(sdMutex);

      //Sau khi upload xong, flush cache của SD card
      sdcard_flush_cb(); // Gọi hàm flush

      // Cập nhật thông tin dung lượng
      cardSize = sd.card()->cardSize() * 512;
      freeSpace = sd.vol()->freeClusterCount() * sd.vol()->sectorsPerCluster() * 512;
      usedSpace = cardSize - freeSpace;
    }
  });

  // Copy select route
  server.on("/copy_select", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      String sourcePath = "/" + filename;

      if (sd.exists(sourcePath.c_str())) {
        SdFile source;
        if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE){
            source.open(sourcePath.c_str(), O_RDONLY);
            if (source.isOpen()) {
              copiedData = ""; // Xóa nội dung cũ
              while (source.available()) {
                copiedData += (char)source.read(); // Đọc vào biến toàn cục
              }
              source.close();
              copiedFilename = filename; // Lưu tên file

              Serial.print("Copied ");
              Serial.print(copiedData.length());
              Serial.print(" bytes from ");
              Serial.println(filename);

              request->send(200, "text/html", "File copied to memory! <a href='/list'>Back to file list</a>");
            } else {
              request->send(500, "text/plain", "Failed to open source file.");
            }
            xSemaphoreGive(sdMutex);
        } else {
            request->send(500, "text/plain", "Failed to acquire mutex for copy.");
        }
      } else {
        request->send(404, "text/plain", "Source file not found.");
      }
    } else {
      request->send(400, "text/plain", "Missing 'file' parameter.");
    }
  });

  // Paste destination route
  server.on("/paste_destination", HTTP_GET, [](AsyncWebServerRequest *request){
    if (copiedFilename != "") {
      String destFilename = copiedFilename;
      int i = 1;
      while (sd.exists(("/" + destFilename).c_str())) {
        destFilename = copiedFilename;
        int dotIndex = destFilename.lastIndexOf('.');
        if (dotIndex > 0) {
          destFilename = destFilename.substring(0, dotIndex) + "_" + String(i) + destFilename.substring(dotIndex); // Sửa
        } else {
          destFilename += "_" + String(i);
        }
        i++;
      }

      String destPath = "/" + destFilename;

      SdFile destination;
      if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
          destination.open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
          if (destination.isOpen()) {
            destination.print(copiedData); // Ghi dữ liệu đã copy vào file mới
            destination.close();
            Serial.println("File pasted to: " + destPath);
            copiedFilename = "";
            copiedData = ""; // Xóa dữ liệu đã copy
            request->send(200, "text/html", "File pasted successfully! <a href='/list'>Back to file list</a>");
          } else {
            request->send(500, "text/plain", "Failed to open destination file for writing.");
          }
          xSemaphoreGive(sdMutex);
      } else {
          request->send(500, "text/plain", "Failed to acquire mutex for paste.");
      }

    } else {
      request->send(400, "text/plain", "No file selected to copy.");
    }
  });

  // Delete route
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      String filepath = "/" + filename;

      if (sd.exists(filepath.c_str())) {
          if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
              if (sd.remove(filepath.c_str())) {
                request->send(200, "text/html", "File deleted successfully! <a href='/list'>Back to file list</a>");
                Serial.println("File deleted: " + filepath);
              } else {
                request->send(500, "text/plain", "Failed to delete file.");
                Serial.println("Failed to delete file: " + filepath);
              }
              xSemaphoreGive(sdMutex);
          } else {
              request->send(500, "text/plain", "Failed to acquire mutex for delete.");
          }
      } else {
        request->send(404, "text/plain", "File not found.");
      }
    } else {
      request->send(400, "text/plain", "Missing 'file' parameter.");
    }
  });

  // New file/folder route
  server.on("/new", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("type") && request->hasParam("name")) {
      String type = request->getParam("type")->value();
      String name = request->getParam("name")->value();
      String path = "/" + name;

      if (type == "file") {
        if (!sd.exists(path.c_str())) {
          SdFile newFile;
          if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
              newFile.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
              if (newFile.isOpen()) {
                newFile.close();
                request->send(200, "text/html", "File created successfully! <a href='/'>Back</a>");
                Serial.println("File created: " + path);
              } else {
                request->send(500, "text/plain", "Failed to create file.");
                Serial.println("Failed to create file: " + path);
              }
              xSemaphoreGive(sdMutex);
          } else {
              request->send(500, "text/plain", "Failed to acquire mutex for new file.");
          }
        } else {
          request->send(400, "text/html", "File already exists.");
          Serial.println("File already exists: " + path);
        }
      } else if (type == "folder") {
        if (!sd.exists(path.c_str())) {
            if (xSemaphoreTake(sdMutex, portMAX_DELAY) == pdTRUE) {
                if (sd.mkdir(path.c_str())) {
                  request->send(200, "text/html", "Folder created successfully! <a href='/'>Back</a>");
                  Serial.println("Folder created: " + path);
                } else {
                  request->send(500, "text/html", "Failed to create folder.");
                  Serial.println("Failed to create folder: " + path);
                }
                xSemaphoreGive(sdMutex);
            } else {
                request->send(500, "text/plain", "Failed to acquire mutex for new folder.");
            }

        } else {
          request->send(400, "text/html", "Folder already exists.");
          Serial.println("Folder already exists: " + path);
        }
      } else {
        request->send(400, "text/html", "Invalid type parameter.");
      }
    } else {
      request->send(400, "text/html", "Missing 'type' or 'name' parameter.");
    }
  });

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}
#endif
