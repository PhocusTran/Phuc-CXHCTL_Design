#ifndef ARDUINO_USB_MODE // Thêm #ifndef ở đây để bao bọc toàn bộ code
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

#define SPI_CS                  10
#define SPI_MOSI                11
#define SPI_MISO                13
#define SPI_SCK                 12

SdFat sd;
SdFile myFile;
Timer timer;

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
USBCDC USBSerial;
#endif

USBMSC MSC;
AsyncWebServer server(80);

// Replace with your network credentials
const char* ssid = "PHUC";
const char* password = "04122002";

// Biến toàn cục
String copiedFilename = "";
String copiedData = ""; // Để lưu trữ nội dung file đã copy

static const uint16_t DISK_SECTOR_SIZE = 512;    // Should be 512
static uint32_t sectors = 0;
static uint32_t readCounter, writeCounter, busyCounter = 0;

// Forward declaration to resolve order dependency
void startWebServer();
void printReadWriteCounter( void ); // forward declare

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize){
  //HWSerial.printf("MSC WRITE: lba: %u, offset: bufsize, bufsize);
  if(sd.card()->isBusy())
    busyCounter++;
  //wait Method
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  {
    writeCounter += bufsize/DISK_SECTOR_SIZE;
    return sd.card()->writeSectors( lba, (uint8_t*)buffer, bufsize/DISK_SECTOR_SIZE)? bufsize: -1;
  }
  else
  {
    writeCounter++;
    return sd.card()->writeSector( lba, (uint8_t*)buffer) ? bufsize : -1;
  }
  //return bufsize;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize){
  //HWSerial.printf("MSC READ: lba: %u, offset: bufsize, bufsize);
  if(sd.card()->isBusy())
    busyCounter++;
  //wait Method
  while(sd.card()->isBusy());
  if(bufsize > DISK_SECTOR_SIZE)
  {
    readCounter += bufsize/DISK_SECTOR_SIZE;
    return sd.card()->writeSectors( lba, (uint8_t*)buffer, bufsize/DISK_SECTOR_SIZE)? bufsize: -1;
  }
  else
  {
    readCounter++;
    return sd.card()->readSector( lba, (uint8_t*)buffer)? bufsize : -1;
  }
  //return bufsize;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void sdcard_flush_cb (void)
{
  if(sd.card()->isBusy())
    busyCounter++;
  while(sd.card()->isBusy());
  sd.card()->syncDevice();

  HWSerial.printf("sdcard_flush_cb");

}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject){
  HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\n", power_condition, start, load_eject);
  return true;
}

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

  //SD init
  HWSerial.print("SD init ... ");
  pinMode(SPI_CS, OUTPUT);
  digitalWrite(SPI_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SdSpiConfig spiConfig(SPI_CS, SHARED_SPI, SD_SCK_MHZ(10)); // 10 MHz
  if (!sd.begin(spiConfig)) {
      HWSerial.println("sd.begin() failed ...");
      while(1); // Halt if SD card initialization fails.
  }
  HWSerial.println("done");

  sectors = sd.card()->sectorCount();
  HWSerial.print("Sectors: ");
  HWSerial.println(sectors);

  //USB MSC Setup
  USB.onEvent(usbEventCallback);
  MSC.vendorID("USBMSC");//max 8 chars
  MSC.productID("USBMSC");//max 16 chars
  MSC.productRevision("1.0");//max 4 chars
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.mediaPresent(true);
  MSC.begin(sectors, DISK_SECTOR_SIZE);
  USBSerial.begin();
  USB.begin();

  //WiFi Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  startWebServer(); // Start the web server AFTER both SD and WiFi are initialized


  timer.setInterval(1000); // Đặt thời gian là 1 giây (1000 ms)
  timer.setCallback(printReadWriteCounter); // Đặt callback function
  timer.start();
}

void printReadWriteCounter( void )
{
  HWSerial.printf("ReadCounter: %d WriteCounter: %d, BusyCounter: %d\r\n", readCounter, writeCounter, busyCounter);
}


void loop() {
  timer.update();  // Update the timer
}

void startWebServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html",
                  "<h1>ESP32 SD Card Web Interface</h1>"
                  "<a href='/list'>List Files</a><br>"
                  "<form action='/upload' method='POST' enctype='multipart/form-data'>"
                  "  Upload File: <input type='file' name='file'><br>"
                  "  <input type='submit' value='Upload'>"
                  "</form>"
                  "<br>"
                  "<form action='/new' method='GET'>"
                  "  New: <select name='type'>"
                  "    <option value='file'>File</option>"
                  "    <option value='folder'>Folder</option>"
                  "  </select>"
                  "  Name: <input type='text' name='name'><br>"
                  "  <input type='submit' value='Create'>"
                  "</form>"
                 );
  });

  // List files route
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String fileList = "<h1>Files on SD Card:</h1><ul>";
    SdFile root; // Sử dụng SdFile
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
    fileList += "</ul><a href='/'>Back</a>";
    request->send(200, "text/html", fileList);
  });

  // Upload file route
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "File uploaded successfully! <a href='/'>Back</a>");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    static SdFile myFile;
    if (!index) {
      Serial.println("Upload Start: " + String(filename));
      myFile.close(); // Đảm bảo file trước đó đã đóng (nếu có)
      if(!myFile.open(("/" + filename).c_str(), O_WRONLY | O_CREAT | O_TRUNC)){  // Sử dụng c_str() và đường dẫn đầy đủ
        Serial.println("Error opening file for writing");
        request->send(500, "text/plain", "Failed to open file for writing");
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
    }
  });

  // Download file route (ĐÃ SỬA ĐỔI)
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
      if (request->hasParam("file")) {
          String filename = request->getParam("file")->value();
          String filepath = "/" + filename;

          if (sd.exists(filepath.c_str())) {
              SdFile downloadFile;
              if (downloadFile.open(filepath.c_str(), O_RDONLY)) {
                  Serial.println("Sending File: " + filename);

                  // Build Header as char array, not String
                  const char* contentType = "application/octet-stream";
                  String header = "Content-Disposition: attachment; filename=\"" + filename + "\"";
                  const char* contentDisposition = header.c_str();

                   request->send(200, contentType, contentDisposition);  // First send, small header
                   uint8_t buffer[2048]; //buffer
                   size_t bytesRead;
                    while ((bytesRead = downloadFile.read(buffer, sizeof(buffer))) > 0)
                     {
                         request->send(200, contentType, buffer, bytesRead); // Chunk send
                     }
                  downloadFile.close();
                  Serial.println("File sent: " + filename);
              } else {
                  request->send(500, "text/plain", "Failed to open file for download.");
              }
          } else {
              request->send(404, "text/plain", "File not found.");
          }
      } else {
          request->send(400, "text/plain", "Missing 'file' parameter.");
      }
  });

  // Copy select route (CHÍNH LÀ PHẦN BỊ THIẾU)
  server.on("/copy_select", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      String filename = request->getParam("file")->value();
      String sourcePath = "/" + filename;

      if (sd.exists(sourcePath.c_str())) {
        SdFile source;
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
      // Create a new filename to avoid overwriting
      String destFilename = copiedFilename;
      int i = 1;
      while (sd.exists(("/" + destFilename).c_str())) {
        destFilename = copiedFilename;
        int dotIndex = destFilename.lastIndexOf('.');
        if (dotIndex > 0) {
          //destFilename.replace(dotIndex, destFilename.length() - dotIndex, "_" + String(i)); // Lỗi ở đây!
          destFilename = destFilename.substring(0, dotIndex) + "_" + String(i) + destFilename.substring(dotIndex); // Sửa
        } else {
          destFilename += "_" + String(i);
        }
        i++;
      }

      String destPath = "/" + destFilename;

      SdFile destination;
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
        if (sd.remove(filepath.c_str())) {
          request->send(200, "text/html", "File deleted successfully! <a href='/list'>Back to file list</a>");
          Serial.println("File deleted: " + filepath);
        } else {
          request->send(500, "text/plain", "Failed to delete file.");
          Serial.println("Failed to delete file: " + filepath);
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
          newFile.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
          if (newFile.isOpen()) {
            newFile.close();
            request->send(200, "text/html", "File created successfully! <a href='/'>Back</a>");
            Serial.println("File created: " + path);
          } else {
            request->send(500, "text/plain", "Failed to create file.");
            Serial.println("Failed to create file: " + path);
          }
        } else {
          request->send(400, "text/html", "File already exists.");
          Serial.println("File already exists: " + path);
        }
      } else if (type == "folder") {
        if (!sd.exists(path.c_str())) {
          if (sd.mkdir(path.c_str())) {
            request->send(200, "text/html", "Folder created successfully! <a href='/'>Back</a>");
            Serial.println("Folder created: " + path);
          } else {
            request->send(500, "text/html", "Failed to create folder.");
            Serial.println("Failed to create folder: " + path);
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
