# SmartPoi ESP32

Firmware for a persistence-of-vision (POV) LED poi. A strip of addressable LEDs spins
with the poi and shows an image column by column, fast enough that the eye sees a
stable picture in the air. Images are stored as compact bit-packed files in LittleFS,
and a small web UI on the poi itself handles configuration, file uploads and OTA
updates.

## Features

- Microsecond-timed POV rendering on a dedicated high-priority FreeRTOS task
- Algorithmic patterns (colour jam, cylon, palettes) and image-based patterns
- Single-character images (`a-z`, `A-Z`, `0-9`, ...) and multi-image sequences
- Real-time pixel streaming over UDP, plus ESP-NOW state sync to auxiliary poi
- Wi-Fi access point or router (STA) mode with a captive-portal style web UI
- File manager and OTA firmware updates over HTTP
- Optional hardware button for mode stepping and blackout (no extra parts required)
- Optional dual-band (2.4 GHz + 5 GHz) Wi-Fi on the ESP32-C5

## Supported boards

Each entry is a PlatformIO environment in `platformio.ini`.

| Environment | Board | LEDs | Optional button | Wi-Fi |
| --- | --- | --- | --- | --- |
| `dfrobot_firebeetle2_esp32s3` | DFRobot FireBeetle 2 ESP32-S3 (APA102) | 36 | no | 2.4 GHz |
| `dfrobot_beetle_esp32c3` | DFRobot Beetle ESP32-C3 (WS2812) | 60 | no | 2.4 GHz |
| `other_c3_board` | Generic ESP32-C3 board (WS2812) - primary target | 36 | yes, GPIO 2 | 2.4 GHz |
| `seeed_xiao_esp32c5` | Seeed Studio XIAO ESP32-C5 (WS2812) | 36 | yes, GPIO 7 | 2.4 + 5 GHz |

Pin, LED count and feature settings live in `platformio.ini`. The GPIOs used by the
button are only claimed when the button is enabled.

## Quick start

```bash
# Build and flash a board
pio run -e other_c3_board -t upload

# Build and upload the LittleFS image (only needed when data/ changes)
pio run -e other_c3_board -t buildfs
pio run -e other_c3_board -t uploadfs

# Serial monitor
pio device monitor
```

On first boot, or whenever `data/settings.txt` is missing, the poi starts its own
access point:

- SSID: `Smart_Poi9`
- Password: `SmartOne`
- Address: <http://192.168.1.1/>

These are the example defaults compiled in from `platformio.ini`
(`AP_NAME` / `AP_PASS`). Change them there before deploying, and set a router
SSID/password through the web UI or `data/settings.txt` for STA mode.

> Do not commit real Wi-Fi credentials. `data/settings.txt` is a plain two-line file
> (`SSID` on line 1, password on line 2) and ships with placeholder values only.

## Build commands

```bash
# One environment at a time (preferred)
pio run -e dfrobot_beetle_esp32c3

# Everything (slow - builds every board)
pio run

# Clean rebuild, normally only after changing a library
pio run -t clean
```

Release binaries are produced by `tests/create_bins.sh` (slow; run it deliberately).

## Patterns

| Pattern | What it shows |
| --- | --- |
| 0 | Live UDP/ESP-NOW pixel feed |
| 1 | Colour jam (algorithmic) |
| 2-5 | Multi-image sequences |
| 6 | Rotates through the stored sequence modes |
| 7 | LEDs off |
| 8-62 | Single characters: `a-z`, `A-Z`, `0-9` |
| 63-69 | Extra single-image glyphs |

The current choice is kept in EEPROM (bytes 10 and 11), so it survives a reboot.

## Images

- Stored in LittleFS as `.bin` files named after a single character (e.g. `/a.bin`).
- Format is 3-3-2 bit-packed: one byte per pixel, 3 bits red, 3 bits green, 2 bits blue.
- Height comes from `NUMPX`; width is derived as `file_size / pxDown`, so keep images
the same height as the LED count.
- `MAXPX` caps the RAM buffer used to decode an image; oversized files are rejected.

Files are uploaded through the web UI (`/edit`), which writes straight into LittleFS
under a disk mutex so the display task never reads a half-written file.

## Optional button

Enable with `-D HAVE_BUTTON=true` and choose `-D BUTTON_PIN=<gpio>`. Wire the button
between the pin and GND; the internal pull-up is used.

| Press | Action |
| --- | --- |
| Short | Steps to the next pattern in the family selected by `button_mode` |
| Long (~0.7 s) | Turns the LEDs off (pattern 7) and toggles `button_mode` |

`button_mode` is a mode, not a boolean:

- `0` - short presses cycle the sequence patterns 1-5
- `1` - short presses cycle the single images 8-62

The mode is derived from the pattern being shown whenever the button acts, so
selecting 1-5 (from the web UI or otherwise) always leaves it at 0, and selecting
8-62 always leaves it at 1. Patterns with no files on the filesystem are skipped, and
the button ignores presses while a file upload or OTA is in progress. Boards with
`HAVE_BUTTON` left at 0 compile the button code out entirely, so the firmware runs
unchanged without one.

## Wi-Fi

- **AP mode** (default when there are no router credentials): serves the web UI at
  `192.168.1.1` and a captive-portal DNS handler.
- **STA / router mode**: joins the SSID and password from `data/settings.txt`.
- **Channel** is stored in EEPROM (byte 13) and can be set with
  `GET /setting?channel=<n>`. Channels 1-13 are always accepted. On the C5, 5 GHz
  channels are also allowed when the current regulatory domain permits them and the
  channel is not DFS (DFS channels cannot be used by a SoftAP).
- **Country** can be set with `GET /setting?country=<ISO>`, e.g. `US` or `ZA`;
  `01` restores ESP-IDF's world domain. It is stored in EEPROM bytes 19-21. Without
  a country, the world domain only permits 5 GHz AP channels 36-48.
- **Dual band** is compiled in with `-D DUAL_BAND_WIFI=true` (C5 only). Router mode
  uses `WIFI_BAND_MODE_AUTO`; AP mode picks the band from `apChannel`.
- **ESP-NOW sync** is 2.4 GHz only, so sync broadcasts are skipped while the AP is on
  a 5 GHz channel. For mixed poi sets, keep the AP on channels 1-13.

## Web API

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | Web UI |
| `/site` | GET | UI page from LittleFS |
| `/list` | GET | List files |
| `/edit` | GET/PUT/POST/DELETE/OPTIONS | Read, create, upload and delete files |
| `/pattern` | GET | Select a pattern |
| `/brightness` | GET | Set brightness |
| `/intervalChange` | GET | Set the sequence interval |
| `/router` | GET | Router/AP mode settings |
| `/setting` | GET | General settings: `ssid`, `pwd`, `channel`, `country` |
| `/returnsettings` | GET | Report current settings |
| `/resetimagetouse` | GET | Restart the image sequence |
| `/get-pixels` | GET | Read back the current frame |
| `/poi-available` | GET | Discovery probe |
| `/elegant` | GET | OTA update page |

The server returns JSON for settings routes and includes CORS headers on file routes.

## Firmware architecture

- `povDisplayTask` (priority 5) - LED timing and pattern rendering
- `fileReaderTask` (priority 2) - LittleFS reads into the shared image buffer
- `webTask` (priority 3) - HTTP server, file manager and OTA
- `bufferMutex` protects the RAM image buffer, `diskMutex` protects the SPI bus/LittleFS
- Static buffers and a 62-entry pattern file cache avoid heap fragmentation
- The 5-second task watchdog requires each task to reset it regularly

## Repository layout

```
src/          Firmware sources (main, tasks, Button, UDPHandler, ...)
include/      Headers and build-time configuration
data/         LittleFS image: web UI, settings.txt, sample images
tests/        Host-side test and release scripts
```

## Tests

```bash
cd tests
./test_wifi_http.sh        # real device, needs nmcli and curl
./test_wifi_http_dryrun.sh # simulated run
```

See `tests/README.md` for details.

## License

No license file is included in this repository; treat it as all rights reserved unless
the owner says otherwise.