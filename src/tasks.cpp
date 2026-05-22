#include "tasks.h"
#include "Globals.h"
#include "LittleFS.h"

// Declare function from ShowLittleFSImage.cpp
extern "C" void markFileForReload(char patternChar);

extern long interval;
#include <EEPROM.h>
#include <FastLED.h>

extern AsyncWebServer server;
extern volatile bool leds_off; // Flag to track if LEDs are already turned off (pattern 7)

String getContentType(String filename) {
  if(filename.endsWith(".htm")) return "text/html";
  if(filename.endsWith(".html")) return "text/html";
  if(filename.endsWith(".css")) return "text/css";
  if(filename.endsWith(".js")) return "application/javascript";
  if(filename.endsWith(".png")) return "image/png";
  if(filename.endsWith(".gif")) return "image/gif";
  if(filename.endsWith(".jpg")) return "image/jpeg";
  if(filename.endsWith(".ico")) return "image/x-icon";
  if(filename.endsWith(".xml")) return "text/xml";
  if(filename.endsWith(".pdf")) return "application/x-pdf";
  if(filename.endsWith(".zip")) return "application/x-zip";
  if(filename.endsWith(".gz")) return "application/x-gzip";
  if(filename.endsWith(".bin")) return "application/octet-stream";
  return "text/plain";
}


unsigned long ota_progress_millis = 0;
TaskHandle_t elegantOTATaskHandle = NULL;

bool checkFileSpace(size_t fileSize) {
  size_t totalSpace = LittleFS.totalBytes();
  size_t maxAllowedSize = totalSpace - MAX_PX - 1024;
  return (fileSize <= maxAllowedSize);
}

/**
 * @brief Get the total space in LittleFS
 * @return size_t Total space in bytes
 */
size_t getTotalSpace() {
  return LittleFS.totalBytes();
}

/**
 * @brief Get the remaining space in LittleFS
 * @return size_t Remaining space in bytes
 */
size_t getRemainingSpace() {
  return LittleFS.totalBytes() - LittleFS.usedBytes();
}

/**
 * @brief Get the used space in LittleFS
 * @return size_t Used space in bytes
 */
size_t getUsedSpace() {
  return LittleFS.usedBytes();
}

// Heap monitoring function
void monitorHeapStatus() {
  static unsigned long lastHeapLog = 0;
  unsigned long now = millis();
  
  // Log heap status every 30 seconds
  if (now - lastHeapLog > 30000) {
    lastHeapLog = now;
    #if SERIAL_DEBUG
    Serial.print("[HEAP] Free: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(", Min Free: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.print(", Max Alloc: ");
    Serial.println(ESP.getMaxAllocHeap());
    #endif
  }
}

String formatBytes(size_t bytes) {
  const char* suffixes[] = {"B", "KB", "MB", "GB"};
  uint8_t i = 0;
  double dblBytes = bytes;
  while(dblBytes >= 1024 && i < 3) {
    dblBytes /= 1024;
    i++;
  }
  return String(dblBytes, 2) + suffixes[i];
}

bool isMemoryAvailableForWebResponse() {
  // Check if we have sufficient free heap for web operations
  size_t freeHeap = ESP.getFreeHeap();
  size_t minFreeHeap = 8192; // Minimum free heap needed for web responses

  if (freeHeap < minFreeHeap) {
    #if SERIAL_DEBUG
    Serial.printf("[MEMORY] Low memory detected: %u bytes free, need %u bytes\n", freeHeap, minFreeHeap);
    #endif
    return false;
  }

  // Also check fragmentation by looking at max allocatable block
  size_t maxAlloc = ESP.getMaxAllocHeap();
  if (maxAlloc < 4096) { // Need at least 4KB contiguous block
    #if SERIAL_DEBUG
    Serial.printf("[MEMORY] Memory fragmentation detected: max alloc %u bytes\n", maxAlloc);
    #endif
    return false;
  }

  return true;
}

// server code:
String loadSiteHtml() {
  File file = LittleFS.open("/site.htm", "r");
  if (!file) {
    #if SERIAL_DEBUG
    Serial.println("Failed to open site.htm");
    #endif
    return "Error loading page";
  }

  String content = file.readString();
  file.close();
  return content;
}

String loadIndexHtml() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    #if SERIAL_DEBUG
    Serial.println("Failed to open index.html");
    #endif
    return "Error loading page";
  }

  String content = file.readString();
  file.close();
  return content;
}

void onOTAStart()
{
  // Log when OTA has started
  #if SERIAL_DEBUG
  Serial.println("OTA update started!");
  #endif
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final)
{
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000)
  {
    ota_progress_millis = millis();
    #if SERIAL_DEBUG
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    #endif
  }
}

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    #if SERIAL_DEBUG
    Serial.println("OTA update finished successfully!");
    #endif
  }
  else
  {
    #if SERIAL_DEBUG
    Serial.println("There was an error during OTA update!");
    #endif
  }
  // <Add your own code here>
}

void handlePatternSettings(AsyncWebServerRequest* request) {
  // Check if we have enough memory for a response
  if (ESP.getFreeHeap() < 8192) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");
  
  if(request->hasArg("patternChooserChange")) {
    int newPatt = request->arg("patternChooserChange").toInt();
    
    if(newPatt < 0 || newPatt > 70) {
        response->setCode(400);
        response->print("{\"Error\":\"Invalid pattern\"}");
        request->send(response);
        return;
    }
    
    patternChooser = newPatt;
    EEPROM.write(10, newPatt);
    
    if(newPatt > 0 && newPatt < 6) {
      pattern = patternChooser;
      EEPROM.write(11, newPatt);
      // Update currentImages for the new pattern
      if (!updateCurrentImagesForPattern(newPatt)) {
        // No files available for this pattern, switch to pattern 1
        pattern = 1;
        patternChooser = 1;
        EEPROM.write(10, 1);
        EEPROM.write(11, 1);
      }
    }
    else if(newPatt == 7) {
      leds_off = false; // Reset flag so povDisplayTask will turn LEDs off
      pattern = patternChooser;
    } else {
      pattern = patternChooser;
      // For patterns 8+, update currentImages and verify file exists
      if (pattern >= 8 && pattern <= 69) {
        if (!updateCurrentImagesForPattern(pattern)) {
          // File doesn't exist for this pattern, switch to pattern 1
          pattern = 1;
          patternChooser = 1;
          EEPROM.write(10, 1);
          EEPROM.write(11, 1);
        }
      }
    }
    
    EEPROM.commit();
    response->setCode(200);
    response->print("{\"Success\":\"Pattern set\"}");
  } else {
    response->setCode(400);
    response->print("{\"Error\":\"Missing parameter\"}");
  }
  request->send(response);
}

void handleRouterSettings(AsyncWebServerRequest* request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  if(request->hasArg("router")) {
    int newRouter = request->arg("router").toInt();
    routerOption = (newRouter == 1);
    EEPROM.write(100, newRouter);
    EEPROM.commit();
    leds_off = false; // Reset flag so povDisplayTask will turn LEDs off
    response->setCode(200);
    response->print("{\"Success\":\"Router mode set\"}");
  } else {
    response->setCode(400);
    response->print("{\"Error\":\"Missing parameter\"}");
  }
  request->send(response);
}

void handleIntervalChange(AsyncWebServerRequest* request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  if(request->hasArg("interval")) {
    long tmp = request->arg("interval").toInt();
    interval = (tmp < 1) ? 500L : 
              (tmp > 1800) ? 1800L * 1000L : 
              tmp * 1000L;
    response->setCode(200);
    response->print("{\"Success\":\"Interval updated\"}");
  } else {
    response->setCode(400);
    response->print("{\"Error\":\"Missing parameter\"}");
  }
  request->send(response);
}

void handleBrightness(AsyncWebServerRequest* request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  if(request->hasArg("brt")) {
    targetBrightness = constrain(request->arg("brt").toInt(), 20, 255);
    // Note: newBrightness will gradually ramp to targetBrightness via checkBrightness()
    // Save target brightness to EEPROM for persistence
    response->setCode(200);
    response->print("{\"Success\":\"Brightness updated\"}");
  } else {
    response->setCode(400);
    response->print("{\"Error\":\"Missing parameter\"}");
  }
  request->send(response);
}

// File operations handlers
void handleFileList(AsyncWebServerRequest *request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");
  
  String path = request->hasArg("dir") ? request->arg("dir") : "/";
  // Ensure path starts with '/' for ESP32 LittleFS
  if(!path.startsWith("/")) {
    path = "/" + path;
  }

  // Stream JSON output directly to response to save memory
  response->print("[");
  
  File root = LittleFS.open(path);
  if(!root) {
    response->print("]");
    request->send(response);
    return;
  }

  File file = root.openNextFile();
  bool firstEntry = true;
  while(file) {
    if(!firstEntry) {
      response->print(",");
    }
    firstEntry = false;
    
    response->print("{\"type\":\"");
    response->print(file.isDirectory() ? "dir" : "file");
    response->print("\",\"name\":\"");
    response->print(String(file.name()));
    response->print("\"}");
    
    file = root.openNextFile();
  }
  
  // Close the root directory
  root.close();
  
  response->print("]");
  request->send(response);
}

void handleFileRead(AsyncWebServerRequest *request) {
  String path = "/" + request->arg("file"); // ESP32 needs leading slash

  if(LittleFS.exists(path)) {
    // Create response with file contents AND headers
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, path, getContentType(path));
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    request->send(response);
  } else {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    // Create error response with headers
    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->setCode(404);
    response->print("File not found");
    request->send(response);
  }
}

void handleFileCreate(AsyncWebServerRequest *request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("text/plain");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  String path = request->arg("path");
  if(path.isEmpty()) {
    response->setCode(400);
    response->print("Bad request");
    request->send(response);
    return;
  }
  
  // Ensure path starts with '/' for ESP32 LittleFS
  if(!path.startsWith("/")) {
    path = "/" + path;
  }

  if(LittleFS.exists(path)) {
    response->setCode(409);
    response->print("File exists");
    request->send(response);
    return;
  }
  // Save current pattern and set to 7 (LEDs off) for file operation
  int savedPattern = pattern;
  pattern = 7;
  leds_off = false; // Reset flag so povDisplayTask will turn LEDs off

  File file = LittleFS.open(path, "w");
  file.close();

  // Restore saved pattern
  pattern = savedPattern;
  response->setCode(200);
  response->print("Created");
  request->send(response);
}

void handleFileDelete(AsyncWebServerRequest *request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("text/plain");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  String path = request->arg("path");
  if(path.isEmpty()) {
    response->setCode(400);
    response->print("Bad request");
    request->send(response);
    return;
  }
  
  // Ensure path starts with '/' for ESP32 LittleFS
  if(!path.startsWith("/")) {
    path = "/" + path;
  }

  // Check if file exists before attempting deletion
  if(!LittleFS.exists(path)) {
    response->setCode(404);
    response->print("File not found");
    request->send(response);
    return;
  }

  // Save current pattern and set to 7 (LEDs off) for file operation
  int savedPattern = pattern;
  pattern = 7;
  leds_off = false; // Reset flag so povDisplayTask will turn LEDs off

  if(!LittleFS.remove(path)) {
    // Restore saved pattern before returning error
    pattern = savedPattern;
    response->setCode(500);
    response->print("Delete failed");
    request->send(response);
    return;
  }

  // Restore saved pattern after successful delete
  pattern = savedPattern;

  // Refresh cache entry if this was a pattern file
  // Pattern files have format like /a.bin where second char is pattern char
  if(path.length() >= 2 && path.endsWith(".bin")) {
    char patternChar = path[1];
    refreshPatternFileCacheEntry(patternChar);
    // Also mark file for reload if it's currently loaded
    markFileForReload(patternChar);
  }

  response->setCode(200);
  response->print("Deleted");
  request->send(response);
}

void handleGeneralSettings(AsyncWebServerRequest* request) {
  // Check if we have enough memory for a response
  if (!isMemoryAvailableForWebResponse()) {
    request->send(503, "text/plain", "Service Unavailable - Low Memory");
    return;
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  response->addHeader("Access-Control-Allow-Credentials", "true");

  // Save current pattern and set to 7 (LEDs off) for file operation
  int savedPattern = pattern;
  pattern = 7;
  leds_off = false; // Reset flag so povDisplayTask will turn LEDs off

  // Handle settings.txt
  // Only update settings.txt if ssid or pwd parameters are provided
  if (request->hasArg("ssid") || request->hasArg("pwd")) {
    File settings = LittleFS.open("/settings.txt", "w");
    if (settings) {
      settings.print(request->arg("ssid") + "\n" + request->arg("pwd"));
      settings.close();
    }
  }
  // Restore saved pattern
  pattern = savedPattern;

  // Channel setting
  if(request->hasArg("channel")) {
    int newChannel = request->arg("channel").toInt();
    EEPROM.write(13, newChannel);
    apChannel = newChannel;
  }

  // Address settings
  const String addresses[] = {"addressA", "addressB", "addressC"};
  const int eepromAddrs[] = {16, 17, 18};
  for(int i=0; i<3; i++) {
    if(request->hasArg(addresses[i])) {
      EEPROM.write(eepromAddrs[i], request->arg(addresses[i]).toInt());
    }
  }

  // Pattern chooser
  if(request->hasArg("patternChooserChange")) {
    int newPatt = request->arg("patternChooserChange").toInt();
    patternChooser = newPatt;
    EEPROM.write(10, newPatt);
    
    if(newPatt > 0 && newPatt < 6) {
      pattern = patternChooser;
      EEPROM.write(11, newPatt);
      // Update currentImages for the new pattern
      if (!updateCurrentImagesForPattern(newPatt)) {
        // No files available for this pattern, switch to pattern 1
        pattern = 1;
        patternChooser = 1;
        EEPROM.write(10, 1);
        EEPROM.write(11, 1);
      }
    }
    else if(newPatt == 70) {
      pattern = patternChooser;
    }
  }

  EEPROM.commit();
  response->setCode(200);
  response->print("{\"Success\":\"Settings updated\"}");
  request->send(response);
}

void clearArray() {
  memset(message1Data, 0, sizeof(message1Data));
}

/**
 * @brief Handles file uploads with platform-specific path handling
 * @note Contains ESP32-specific path prefix logic
 * @todo Verify CORS handling for upload endpoint
 */
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File fsUploadFile;
    static size_t totalFileSize = 0;
    static int savedPattern = -1;  // Static variable to preserve saved pattern across calls
    static char fullPath[16];      // Static buffer for full path (max: "/a.bin" + null)

    if(!index) { // Start of upload
        // Save current pattern and set to 7 (LEDs off) for file operation
        savedPattern = pattern;
        pattern = 7;
        leds_off = false; // Reset flag so povDisplayTask will turn LEDs off

        // Set upload flag to disable FastLED operations
        uploadInProgress = true;
        // Clear memory and reset tracking
        clearArray();
        totalFileSize = 0;

        // Clean up FastLED/RMT before upload to prevent channel state errors
        FastLED.clear();
        FastLED.show();
        delay(50);  // Allow RMT channel to complete any pending operations

        // Build full path in buffer (max 16 chars for safety)
        snprintf(fullPath, sizeof(fullPath), "/%s", filename.c_str());

        // Validate filename format
        if (strlen(fullPath) != 6 || images.indexOf(fullPath[1]) == -1) {
            // Reset upload state before returning error
            uploadInProgress = false;
            if (savedPattern != -1) {
                pattern = savedPattern;
                savedPattern = -1;
            }
            request->send(400, "text/plain", "Invalid filename");
            return;
        }

        // Check remaining space before opening file
        size_t remainingSpace = getRemainingSpace();
        if(request->contentLength() > remainingSpace ||
           request->contentLength() > MAX_PX) {
            // Reset upload state before returning error
            uploadInProgress = false;
            if (savedPattern != -1) {
                pattern = savedPattern;
                savedPattern = -1;
            }
            request->send(507, "text/plain", "File size exceeds limit");
            return;
        }

        // Attempt to open file
        fsUploadFile = LittleFS.open(fullPath, "w");
        if(!fsUploadFile) {
            // Reset upload state before returning error
            uploadInProgress = false;
            if (savedPattern != -1) {
                pattern = savedPattern;
                savedPattern = -1;
            }
            request->send(500, "text/plain", "Upload failed");
            return;
        }
    }

    // Write received data
    if(len > 0 && fsUploadFile) {
        // Check cumulative size during write
        totalFileSize += len;
        if(totalFileSize > MAX_PX ||
           totalFileSize > getRemainingSpace()) {
            fsUploadFile.close();
            LittleFS.remove(fullPath);
            // Reset upload state before returning error
            uploadInProgress = false;
            if (savedPattern != -1) {
                pattern = savedPattern;
                savedPattern = -1;
            }
            request->send(507, "text/plain", "File size exceeds limit");
            return;
        }
        
        fsUploadFile.write(data, len);
    }
    // Finalize upload
    if(final && fsUploadFile) {
        fsUploadFile.close();
        delay(10);  // Allow file system operations to complete
        uploadInProgress = false;  // Re-enable FastLED operations
        
        // Restore saved pattern
        if(savedPattern != -1) {
            pattern = savedPattern;
            savedPattern = -1;  // Reset for next upload
        }
        
        // Update current images for the current pattern after file upload
        updateCurrentImagesForPattern(pattern);
        
        // Refresh cache entry for this pattern file
        // Pattern files have format like /a.bin where second char is pattern char
        if(strlen(fullPath) >= 2) {
            char patternChar = fullPath[1];
            refreshPatternFileCacheEntry(patternChar);
            // Also mark file for reload if it's currently loaded
            markFileForReload(patternChar);
        }
    }
    // Handle aborted uploads
    if(!final && !fsUploadFile) {
        // Reset upload state before returning error
        uploadInProgress = false;
        if (savedPattern != -1) {
            pattern = savedPattern;
            savedPattern = -1;
        }
        request->send(500, "text/plain", "Upload aborted");
    }
}

/////////////////////////////////////////////// end elegantOTA code //////////////////////////////////////

/**
 * @brief Sets up ElegantOTA task for WiFi firmware updates and Server
 */
void setupElegantOTATask()
{
  xTaskCreatePinnedToCore(
      elegantOTATask,        // Task function
      "Elegant OTA Task",    // Name of the task
      8192,                  // Stack size increased from 4096 to 8192 words (32KB)
      NULL,                  // Task input parameter
      WEB_TASK_PRIO,         // Priority of the task (3 per blueprint)
      &elegantOTATaskHandle, // Task handle
      0                      // Core where the task should run (core 1)
  );
}

/**
 * @brief Periodically checks for WiFi firmware updates and handles Web Server
 * @details Server handling also happens here
 */
void elegantOTATask(void *pvParameters)
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", loadSiteHtml());
  });

  server.on("/site", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/"); // Default alternative
  });

  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {                                                                               
    request->redirect("/");  // Android captive portal check                                                                                              
  });                                                                                                                                                     
                                                                                                                                                          
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {                                                                        
    request->redirect("/");  // Apple captive portal check                                                                                                
  });                                                                                                                                                     
                                                                                                                                                          
  server.on("/connectivity-check.html", HTTP_GET, [](AsyncWebServerRequest *request) {                                                                    
    request->redirect("/");  // Windows/Linux captive portal check                                                                                        
  }); 

  server.on("/elegant", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", loadIndexHtml());
  });

  //NOTE: handled by ElegantOTA:
  // "/update" *OTA UPDATE
  
  // Migrated routes from WebServerSetup.cpp
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleFileList(request);
  });

  server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(request->hasArg("file")) {
      handleFileRead(request);
    } else {
      request->send(400, "text/plain", "Missing file parameter");
    }
  });

  server.on("/edit", HTTP_PUT, [](AsyncWebServerRequest *request) {
    if(request->hasArg("path")) {
      handleFileCreate(request);
    } else {
      request->send(400, "text/plain", "Missing path parameter");
    }
  });

  server.on("/edit", HTTP_POST, 
    [](AsyncWebServerRequest *request) {
      // Handle preflight response
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain");
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      response->addHeader("Access-Control-Allow-Credentials", "true");
      request->send(response);
    },
    handleFileUpload,  // Upload handler (4th param)
    NULL               // Body handler (5th param)
  );

  // Add specific OPTIONS handler for DELETE preflight on /edit
  server.on("/edit", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    request->send(response);
  });

  server.on("/edit", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    if(request->hasArg("path")) {
      handleFileDelete(request);
    } else {
      request->send(400, "text/plain", "Missing path parameter");
    }
  });

  server.on("/get-pixels", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->print(String(NUM_PX));
    request->send(response);
  });

  server.on("/poi-available", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->print("1");
    request->send(response);
  });

  server.on("/options", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    request->send(response);
  });

  // Existing migrated routes (keep these):
  server.on("/resetimagetouse", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    imageToUse = 0;
    previousMillis3 = millis();
    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->print("");
    request->send(response);
  });

  server.on("/returnsettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->send(503, "text/plain", "Service Unavailable - Low Memory");
      return;
    }

    AsyncResponseStream* response = request->beginResponseStream("text/html");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, FETCH");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Credentials", "true");
    
    File settings = LittleFS.open("/settings.txt", "r");
    // Use efficient string building to reduce memory fragmentation
    char buffer[256]; // Should be enough for: ssid(32),pwd(32),channel(2),addresses(3*3),pattern(2) + separators
    String ssid = settings.readStringUntil('\n');
    String pwd = settings.readStringUntil('\n');
    settings.close();

    // Use snprintf for efficient string building
    snprintf(buffer, sizeof(buffer), "%s,%s,%d,%d,%d,%d,%d,%d",
             ssid.c_str(), pwd.c_str(),
             apChannel, addrNumA, addrNumB, addrNumC, addrNumD, patternChooser);

    response->print(buffer);
    request->send(response);
  });

  server.on("/pattern", HTTP_GET, [](AsyncWebServerRequest *request) {
    handlePatternSettings(request);
  });

  server.on("/router", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleRouterSettings(request);
  });

  server.on("/intervalChange", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleIntervalChange(request);
  });

  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleBrightness(request);
  });

  server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleGeneralSettings(request);
  });

  // notFound handler (for captive portal)
  server.onNotFound([](AsyncWebServerRequest *request) {
    // Check if we have enough memory for a response
    if (!isMemoryAvailableForWebResponse()) {
      request->redirect("/");
      return;
    }

    // For captive portal, just redirect to root
    request->redirect("/");
  });

  
  server.begin();
  ElegantOTA.begin(&server); // Start ElegantOTA

  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  // loop handling ElegantOTA:
  for (;;)
  {
    ElegantOTA.loop(); // OTA updates: see https://randomnerdtutorials.com/esp32-ota-over-the-air-vs-code/ for usage
     vTaskDelay(100 / portTICK_PERIOD_MS); // Yield to other tasks
  }
}