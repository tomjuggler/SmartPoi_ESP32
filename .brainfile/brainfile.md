schema: https://brainfile.md/v2/board.json
title: SmartPoi ESP32 LED Controller
agent:
  instructions:
    - Task files are individual .md files in board/
    - Completed tasks are in logs/
    - Preserve all IDs
    - Make minimal changes
columns:
  - id: todo
    title: To Do
  - id: in-progress
    title: In Progress
---

# SmartPoi ESP32 LED Controller

ESP32-based LED poi controller supporting multiple patterns, WiFi control, OTA updates, and image display.

## Key Features
- Supports multiple ESP32 boards (FireBeetle 2 ESP32-S3, Beetle ESP32-C3)
- FastLED-based LED control for APA102 or WS2812 strips
- Multiple display patterns (70 total: UDP control, color patterns, image sequences)
- Web-based control interface with file management
- OTA firmware updates via ElegantOTA
- LittleFS filesystem for image storage
- WiFi modes: AP or Station with router configuration
- UDP control for real-time LED manipulation
- SmartPoi API check-in service
- Brightness control with smooth transitions

## Project Structure
- `src/` - Main application code
- `include/` - Header files
- `data/` - Web interface files and images
- PlatformIO-based build system with multiple board configurations

> Note: Completing a task moves it to `logs/` via `brainfile complete`.
