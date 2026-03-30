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