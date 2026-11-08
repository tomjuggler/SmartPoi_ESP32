# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a SmartPoi (Persistence of Vision) project for ESP32 microcontrollers that creates LED displays using spinning poi. The system supports multiple patterns, WiFi connectivity, OTA updates, and file management via a web interface.

## Architecture

### Core Architecture
- **Multi-task FreeRTOS system**: Three main tasks with different priorities:
  - `povDisplayTask` (PRIO 5): High-priority task for microsecond-accurate LED timing
  - `fileReaderTask` (PRIO 2): Background task for LittleFS file operations
  - `webTask` (PRIO 3): Web server task (created in `setupElegantOTATask()`)
- **Thread safety**: Uses FreeRTOS mutex semaphores (`bufferMutex`, `diskMutex`) to protect shared resources
- **Memory management**: Uses static buffers and caching to avoid heap fragmentation

### Key Components
1. **LED Control**: Uses FastLED library with APA102 or WS2812 LEDs (configurable via `LED_APA102`)
2. **File System**: LittleFS for storing pattern files (`.bin` format) and settings
3. **Network**: WiFi in AP or STA mode, UDP for real-time pattern control, HTTP web server for configuration
4. **Pattern System**: Supports multiple pattern types (0-69) with file-based and algorithmic patterns

### Pattern System
- **Pattern 0**: UDP real-time control
- **Pattern 1**: Color jam algorithmic pattern
- **Pattern 2-5**: Multi-image sequences using different character sets
- **Pattern 7**: Black (LEDs off)
- **Pattern 8-69**: Single character patterns (a-z, A-Z, 0-9)
- **File caching**: `patternFileExistsCache` avoids repeated LittleFS.exists() calls

### Image Format
- Images stored as `.bin` files in 3-3-2 bit-packed format (8 bits per pixel)
- File naming: single character filenames (e.g., `/a.bin`, `/b.bin`)
- Image dimensions calculated dynamically: `pxAcross = file_size / pxDown`

## Development Commands

### PlatformIO Commands
```bash
# Build for all environments (do not use this) 
pio run 

# Build for specific environment, preferable (use default)
pio run -e dfrobot_beetle_esp32c3

# Upload to connected device
pio run -t upload

# Monitor serial output
pio device monitor

# Clean build - usually not needed unless we change a library
pio run -t clean

# Build filesystem image - filesystem does not change much, unless we are editing data folder
pio run -t buildfs

# Upload filesystem
pio run -t uploadfs
```

### Testing
```bash
# Run WiFi/HTTP test scripts (requires NetworkManager and curl)
cd tests
./test_wifi_http.sh          # Actual test - may be old/not relevant
./test_wifi_http_dryrun.sh   # Dry-run simulation (not relevant?)
```

### Release Creation
```bash
# Create release binaries (see tests/create_bins.sh) - only do this if specifically prompted, takes a long time
cd tests
./create_bins.sh
```

## Configuration

### PlatformIO Configuration (`platformio.ini`)
- Multiple environments for different ESP32 boards
- Key build flags:
  - `DATAPIN`, `CLOCKPIN`: LED data/clock pins
  - `NUMLEDS`, `NUMPX`: Number of LEDs (NUMLEDS = NUMPX + 1 for some WS2812 strips)
  - `MAXPX`: Maximum image size (memory allocation)
  - `LED_APA102`: true for APA102, false for WS2812
  - `auxillary`: false for main poi, true for auxiliary units
  - `C_THREE`: true for ESP32-C3 boards

### WiFi Configuration
- AP mode: SSID `Smart_Poi9`, password `SmartOne`, IP `192.168.1.1`
- STA mode: Configurable via web interface and data/settings.txt file
- Settings stored in EEPROM and LittleFS

## Memory Management

### Critical Constraints
- `MAXPX` defines maximum image buffer size in RAM
- Pattern file cache (62 entries) avoids filesystem overhead
- Static shadow buffers in tasks prevent heap fragmentation
- Task stack sizes defined in `Globals.h`:
  - `POV_TASK_STACK_SIZE`: 8192
  - `FILE_TASK_STACK_SIZE`: 4096
  - `WEB_TASK_STACK_SIZE`: 4096

### Watchdog Timer
- 5-second task watchdog configured for all tasks
- Tasks must call `esp_task_wdt_reset()` regularly

## File Structure

```
src/
├── main.cpp              # Entry point, task creation, pattern logic
├── tasks.cpp             # Web server, HTTP handlers, OTA setup
├── Initialize.cpp        # Hardware initialization, EEPROM functions
├── ColourPalette.cpp     # Color manipulation functions
├── ShowLittleFSImage.cpp # Image display utilities
├── TimeFunc.cpp          # Timing functions
└── UDPHandler.cpp        # UDP packet handling

include/
├── Globals.h            # Global constants, extern declarations
├── tasks.h              # Task function prototypes
├── Initialize.h         # Initialization prototypes
├── ColourPalette.h      # Color function prototypes
├── ShowLittleFSImage.h  # Image display prototypes
├── TimeFunc.h           # Timing function prototypes
└── UDPHandler.h         # UDP handler prototypes

data/                    # Web interface files, settings, images
tests/                   # Test scripts
releases/                # Release binaries
```

## Important Code Patterns

### Mutex Usage
```cpp
// Protect RAM buffer
if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
  // Access message1Data
  xSemaphoreGive(bufferMutex);
}

// Protect filesystem access
if (xSemaphoreTake(diskMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
  // Access LittleFS
  xSemaphoreGive(diskMutex);
}
```

### Pattern Switching
Use `updateCurrentImagesForPattern()` to validate and set patterns. Returns `false` if no files exist for the pattern.

### Image Loading
File reader task monitors `pattern` and `imageToUse` variables, loads appropriate `.bin` file into `message1Data` buffer.

## Common Issues

1. **Memory fragmentation**: Use static buffers, avoid dynamic allocation in loops
2. **Filesystem errors**: Cache file existence checks, use mutex for SPI bus access
3. **LED timing**: POV task must maintain microsecond timing for smooth display
4. **WiFi conflicts**: Disable LED operations during uploads (`uploadInProgress` flag)

## Board Support

1. **DFRobot FireBeetle 2 ESP32-S3**: Old target, APA102 LEDs
2. **DFRobot Beetle ESP32-C3**: Secondary target, WS2812 LEDs
3. **Other ESP32-C3 boards**: Primary Target, WS2812 LEDs by default
4. **Seeed Studio XIAO ESP32-C5** (`seeed_xiao_esp32c5`): dual-band 2.4 GHz + 5 GHz Wi-Fi 6, WS2812 LEDs on `DATAPIN` (XIAO D0 / GPIO1), 8 MB flash / 8 MB PSRAM

### Dual-band (5 GHz) Wi-Fi

The ESP32-C5 radio supports 5 GHz, the older boards do not. This is wired up as follows:

- The `seeed_xiao_esp32c5` environment is pinned to Arduino core 3.3.12 / ESP-IDF 5.5.5 (`platform = .../55.03.312-1/...`) because ESP32-C5 support does not exist in 51.03.04. The other environments stay on 51.03.04 so their builds are unchanged.
- `-D DUAL_BAND_WIFI=true` enables the band logic (see `wifiChannelIs5GHz()` / `wifiChannelIsValid()` in `include/Globals.h`). On the C3/S3 environments it defaults to `0` and all band calls are compiled out.
- **Router (STA) mode**: `WiFi.setBandMode(WIFI_BAND_MODE_AUTO)` after `WiFi.mode()`, so the C5 can join either a 2.4 GHz or a 5 GHz access point.
- **AP mode**: the radio uses 5 GHz when `apChannel` is a 5 GHz channel (36-64, 100-144, 149-177, multiples of 4) and 2.4 GHz otherwise. `apChannel` is stored in EEPROM address 13 and can be set through `GET /setting?channel=<n>` (`handleGeneralSettings`). Invalid or 5 GHz channels on non-C5 boards fall back to channel 1.
- **Default AP channel**: `-D DEFAULT_AP_CHANNEL=<n>` in `platformio.ini` is used the first time (EEPROM erased/invalid). The C5 env sets `36` so a fresh poi brings its setup AP up on 5 GHz. Change it at runtime with `/setting?channel=<n>` (note: the route is `/setting`, not `/settings`).
- **Regulatory country**: set at runtime with `GET /setting?country=<ISO>`, e.g. `US` or `ZA` (`01` restores the world domain). The code is stored in EEPROM addresses 19-21 and applied before the AP/STA starts; `-D WIFI_COUNTRY=\"XX\"` provides a build-time fallback. Without it ESP-IDF's world domain (`01`) rejects 5 GHz AP channels above 48, so only 36-48 are usable. With `country=US`, for example, 149-165 become available; the country channel mask is read back with `esp_wifi_get_country()`.
- **`/setting?channel=<n>` is validated**: it accepts 1-13 and, on the C5, 5 GHz channels that the current country allows and that are not DFS. Invalid or disallowed channels return HTTP 400 with `{"Error":"Channel not allowed in this country"}` instead of silently falling back to channel 1. A stored channel that becomes invalid after a country change is replaced by `DEFAULT_AP_CHANNEL` at the next AP start.
- **DFS channels are not usable for the SoftAP**: IDF rejects channels 52-64 and 100-144 in AP mode (`Configure channel to a DFS channel is not allowed in ap mode`). `wifiChannelIsValid()` therefore treats them as invalid and the AP falls back to channel 1. Use 36-48 or 149-165 for 5 GHz APs.
- **Compatibility**: keep the AP on channel 1-13 for mixed poi sets. Older C3/S3 units cannot see a 5 GHz AP, and ESP-NOW (used for main-to-aux sync) is 2.4 GHz only, so `broadcastSync()` is skipped on a 5 GHz AP.
- `WiFi.setBandMode()` must be called **after** `WiFi.mode()`, which is what starts the radio (otherwise the Arduino API logs "You need to start WiFi first").