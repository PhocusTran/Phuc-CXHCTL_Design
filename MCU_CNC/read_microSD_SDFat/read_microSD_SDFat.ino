 #include <SPI.h>
 #include <SdFat.h>
 #include <WiFi.h>
 #include <WebServer.h>
 #include <AsyncTCP.h>
 #include <ESPAsyncWebServer.h>

 #define CS 10  // Corrected CS pin to 5

 // Thay đổi kiểu dữ liệu thành SdFile
 SdFat SD;
 SdFile myFile;

 // Replace with your network credentials
 const char* ssid = "PHUC";
 const char* password = "04122002";

 AsyncWebServer server(80);

 // Biến toàn cục (CỰC KỲ CẨN THẬN!)
 String copiedFilename = "";
 String copiedData = ""; // Để lưu trữ nội dung file đã copy

 void setup() {
  Serial.begin(115200);

  // Initialize SD card
  Serial.print("Initializing SD card...");
  // Sử dụng SD.begin từ SdFat
  if (!SD.begin(CS, SD_SCK_MHZ(10))) { // hoặc SD_SCK_MHZ(50) tùy vào tốc độ SPI phù hợp
   Serial.println("initialization failed!");
   while (1);
  }
  Serial.println("initialization done.");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Define routes

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
   if (!root.open("/")) {
    fileList += "<li>Failed to open root directory</li>";
   } else {
    SdFile entry;
    while (entry.openNext(&root, O_RDONLY)) { //Sửa
     char filenameBuffer[32];
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

  // Download route
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
  Serial.println("Received download request");
  if (request->hasParam("file")) {
   String filename = request->getParam("file")->value();
   String filepath = "/" + filename;
   Serial.print("Filepath: ");
   Serial.println(filepath);
   Serial.print("Free Heap before check: ");
   Serial.println(ESP.getFreeHeap());

   if (SD.exists(filepath.c_str())) {
    Serial.println("File exists");
    Serial.print("Free Heap before open file: ");
    Serial.println(ESP.getFreeHeap());
    SdFile downloadFile;
    if (downloadFile.open(filepath.c_str(), O_RDONLY)) {
     Serial.println("File opened successfully");
     Serial.print("Free Heap before get file size: ");
     Serial.println(ESP.getFreeHeap());

     size_t fileSize = downloadFile.fileSize();
     Serial.print("File size: ");
     Serial.println(fileSize);
     Serial.print("Free Heap before create response: ");
     Serial.println(ESP.getFreeHeap());

     // Use beginResponseStream correctly.
     AsyncResponseStream *response = request->beginResponseStream("application/octet-stream", fileSize); // No filename here
        response->setContentLength(fileSize);

     // Add Content-Disposition header for file download.
        String headerValue = "attachment; filename=\"" + filename + "\"";
        response->addHeader("Content-Disposition",headerValue);

        // Send the headers and response to client
        request->send(response);  // SEND Response

         uint8_t buffer[1024]; // Increase buffer size
         size_t bytesRead;
         while ((bytesRead = downloadFile.read(buffer, sizeof(buffer))) > 0)
         {
           Serial.print("Sending data chunk: ");
           Serial.println(bytesRead);
         response->write(buffer, bytesRead);

         }

        //Close the file and end the send
        downloadFile.close();

        Serial.println("Response sent");
     Serial.print("Free Heap after send response: ");
     Serial.println(ESP.getFreeHeap());
   } else {
    Serial.println("Failed to open file");
    request->send(500, "text/plain", "Failed to open file for download.");
   }
  } else {
   Serial.println("File not found");
   request->send(404, "text/plain", "File not found.");
  }
  Serial.print("Free Heap after send file: ");
  Serial.println(ESP.getFreeHeap());
 } else {
  Serial.println("Missing 'file' parameter");
  request->send(400, "text/plain", "Missing 'file' parameter.");
 }
 });

  // Copy select route
  server.on("/copy_select", HTTP_GET, [](AsyncWebServerRequest *request){
   if (request->hasParam("file")) {
    String filename = request->getParam("file")->value();
    String sourcePath = "/" + filename;

    if (SD.exists(sourcePath.c_str())) {
     SdFile source;
     if (source.open(sourcePath.c_str(), O_RDONLY)) {
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
    while (SD.exists(("/" + destFilename).c_str())) {
     destFilename = copiedFilename;
     int dotIndex = destFilename.lastIndexOf('.');
     if (dotIndex > 0) {
      destFilename = destFilename.substring(0, dotIndex) + "_" + String(i) + destFilename.substring(dotIndex);
     } else {
      destFilename += "_" + String(i);
     }
     i++;
    }

    String destPath = "/" + destFilename;

    SdFile destination;
    if(destination.open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
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

    if (SD.exists(filepath.c_str())) {
     if (SD.remove(filepath.c_str())) {
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
     if (!SD.exists(path.c_str())) {
      SdFile newFile;
      if (newFile.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC)) {
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
     if (!SD.exists(path.c_str())) {
      if (SD.mkdir(path.c_str())) {
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

 void loop() {
  // Nothing to do here in the main loop
 }
