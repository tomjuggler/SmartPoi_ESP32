# SmartPoi ESP32

> ## Active development
>
> **Magic Poi is here now.** Development is supported by Patreon subscribers -
> subscribe for free to receive regular updates.
>
> **Patreon:** <https://patreon.com/c/circusscientist>  
> **Magic Poi:** <http://magicpoi.com/>

Persistence-of-vision (POV) LED poi firmware for ESP32. A strip of addressable LEDs
spins with the poi and shows images in the air. The **Android app is the main
controller**; the on-board web UI is a backup with only basic controls (pattern,
brightness, interval and AP/router settings) - it does not do file management.

Supported boards: **ESP32-C3, ESP32-S3 and ESP32-C5**.
Image heights of **36, 60, 72 and 120 px** are supported - the LED count in
`platformio.ini` is simply whatever the current test board has fitted.

## Control app

**Android Controls** - the primary way to run a poi, including uploading and resizing
images before they are sent. Download from Google Play:
<https://play.google.com/store/apps/details?id=com.circusscientist.smartpoicontrols>

## Features

- Android app control: patterns, brightness, interval, image upload and resizing
- LittleFS image storage with over 60 images on board
- Backup web UI for basic settings
- Hardware button support on all variants (stepping patterns and blackout)
- Real-time pixel streaming over UDP
- OTA firmware updates with ElegantOTA
- ESP-NOW state sync to auxiliary poi
- Dual-band 2.4 GHz + 5 GHz Wi-Fi on the ESP32-C5

## Before you upload

Set the router credentials **before** flashing the filesystem, so the poi joins your
network on first boot:

- Edit `data/settings.txt` - line 1 is the SSID, line 2 is the password - then upload
  the filesystem (commands below). The same values can also be entered in the backup
  web UI (`/setting?ssid=...&pwd=...`).
- With no `settings.txt`, the poi starts its own access point:

- SSID: `Smart_Poi_10`
- Password: `SmartOne`
- Address: <http://192.168.1.1/>

These are example defaults compiled in from `platformio.ini` (`AP_NAME` /
`AP_PASS`). Change them there before deploying.

> Do not commit real Wi-Fi credentials. `data/settings.txt` ships with placeholder
> values only.

## Quick start

```bash
# 1. Set data/settings.txt first (SSID on line 1, password on line 2)

# 2. Build and flash the firmware
pio run -e other_c3_board -t upload

# 3. Upload the filesystem image when data/ changes
pio run -e other_c3_board -t buildfs
pio run -e other_c3_board -t uploadfs

# 4. Serial monitor
pio device monitor
```

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
| 0 | Live UDP pixel stream (UDP only, not ESP-NOW) |
| 1 | Colour jam (algorithmic) |
| 2-5 | Multi-image sequences |
| 6 | Rotates through the stored sequence modes |
| 7 | LEDs off |
| 8+ | Stored images - over 60 available |

ESP-NOW carries pattern state between poi, never pixel data. The current pattern is
remembered in EEPROM only for the sequence modes (1-6); a single-image choice is not
restored after a reboot and falls back to mode 1.

## Images

- Stored in LittleFS as `.bin` files (for example `/a.bin`, `/b.bin`).
- Format is 3-3-2 bit-packed: one byte per pixel, 3 bits red, 3 bits green, 2 bits blue.
- Heights of 36, 60, 72 and 120 px are supported.
- The Android app resizes images for the selected poi before upload, so you do not
  need to match the LED count yourself.
- `MAXPX` caps the RAM buffer used to decode an image; oversized files are rejected.

Uploads go through the HTTP file routes (`/edit`), which write into LittleFS under a
disk mutex so the display task never reads a half-written file.

## Button

The button works on every board variant; enable it with `-D HAVE_BUTTON=true` and
choose `-D BUTTON_PIN=<gpio>`. Wire the button between the pin and GND; the internal
pull-up is used.

| Press | Action |
| --- | --- |
| Short | Steps to the next pattern in the family selected by `button_mode` |
| Long (~0.7 s) | Turns the LEDs off (pattern 7) and toggles `button_mode` |

`button_mode` is a mode, not a boolean:

- `0` - short presses cycle the sequence patterns 1-5
- `1` - short presses cycle the stored images from 8 upwards

The mode is derived from the pattern being shown whenever the button acts, so
selecting 1-5 always leaves it at 0, and selecting an image leaves it at 1. Patterns
with no files on the filesystem are skipped, and the button ignores presses while a
file upload or OTA is in progress. With `HAVE_BUTTON` left at 0 the button code is
compiled out entirely, so the firmware runs unchanged without one.

## Wi-Fi

- **AP mode** (default when there are no router credentials): serves the backup web UI
  at `192.168.1.1` with a captive-portal DNS handler.
- **STA / router mode**: joins the SSID and password from `data/settings.txt`.
- **Channel** is stored in EEPROM (byte 13) and can be set with
  `GET /setting?channel=<n>`. Channels 1-13 are always accepted. On the C5, 5 GHz
  channels are also allowed when the regulatory domain permits them and the channel
  is not DFS (DFS channels cannot be used by a SoftAP).
- **Country** can be set with `GET /setting?country=<ISO>`, e.g. `US` or `ZA`;
  `01` restores ESP-IDF's world domain. It is stored in EEPROM bytes 19-21. Without
  a country, the world domain only permits 5 GHz AP channels 36-48.
- **Dual band** is compiled in with `-D DUAL_BAND_WIFI=true` (C5 only). Router mode
  uses `WIFI_BAND_MODE_AUTO`; AP mode picks the band from `apChannel`.
- **ESP-NOW sync** is 2.4 GHz only, so sync broadcasts are skipped while the AP is on
  a 5 GHz channel. For mixed poi sets, keep the AP on channels 1-13.

## HTTP API

Used by the Android app and available for scripting. The web UI only exposes a subset.

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | Backup web UI |
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

---

**Magic Poi is available now.** Active development is supported by Patreon
subscribers - subscribe for free to receive regular updates:
<https://patreon.com/c/circusscientist> | <http://magicpoi.com/>
Under active development.