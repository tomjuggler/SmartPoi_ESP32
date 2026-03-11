# SmartPoi WiFi and HTTP Test Scripts

This directory contains test scripts for testing WiFi connectivity and HTTP requests to a SmartPoi device.

## Scripts

### 1. `test_wifi_http.sh` - Main Test Script

This script performs the following steps:

1. **Connect to SmartPoi WiFi**: Uses `nmcli` to connect to the SmartPoi's Access Point
   - SSID: `Smart_Poi9` (from `main.cpp`)
   - Password: `SmartOne` (from `main.cpp`)
   - IP Address: `192.168.1.1` (default AP IP)

2. **Test HTTP Connectivity**: Verifies the HTTP server on port 80 is responding

3. **List Files**: Calls the `/list` endpoint to get a list of files on the SmartPoi

4. **Delete a .bin File**: Finds a `.bin` file and attempts to delete it using the `/edit` endpoint with DELETE method

5. **Verify Deletion**: Lists files again to confirm the file was deleted

**Usage:**
```bash
./test_wifi_http.sh
```

**Requirements:**
- NetworkManager with `nmcli` command
- `curl` for HTTP requests
- A running SmartPoi device in AP mode (wifiModeChooser == 1)

### 2. `test_wifi_http_dryrun.sh` - Dry-run Simulation

This script simulates the entire test process without actually connecting to a device. Useful for:
- Testing the script logic
- Demonstrating the expected flow
- Training purposes

**Usage:**
```bash
./test_wifi_http_dryrun.sh
```

## HTTP Endpoints Tested

Based on the code in `tasks.cpp`, the script tests these endpoints:

1. **Root (`/`)**: Basic HTTP connectivity test
2. **`/list`**: Lists files in LittleFS
3. **`/edit`**: File operations (DELETE method with `path` parameter)

## Code References

### WiFi Configuration (from `main.cpp`)
```cpp
const char *apName = "Smart_Poi9";
const char *apPass = "SmartOne";
IPAddress apIP(192, 168, 1, 1);
```

### HTTP Endpoints (from `tasks.cpp`)
- `server.on("/list", HTTP_GET, handleFileList)` - Line 590
- `server.on("/edit", HTTP_DELETE, handleFileDelete)` - Line 634

### File Deletion Handler (from `tasks.cpp`)
The `handleFileDelete` function (line 351) expects a `path` parameter and returns:
- `200` with "Deleted" on success
- `400` with "Bad request" if path is empty
- `500` with "Delete failed" if file removal fails

## Troubleshooting

### nmcli Connection Issues
If you get "key-mgmt: property is missing" error, the script includes fallback logic that:
1. First tries standard connection
2. Falls back to creating a connection profile with explicit `wpa-psk` key management

### HTTP Connection Issues
- Ensure the SmartPoi is in AP mode (wifiModeChooser == 1)
- Check that the HTTP server is running (should start with ElegantOTA task)
- Verify the IP address matches your SmartPoi configuration

### File Deletion Issues
- The script looks for `.bin` files specifically
- Files must be in the root directory (`/`)
- The deletion requires CORS headers which are handled in the code

## Safety Features

1. **Confirmation Prompt**: Asks for confirmation before deleting any file
2. **File Verification**: Verifies file exists before attempting deletion
3. **Deletion Verification**: Lists files again to confirm deletion succeeded
4. **Error Handling**: Each step has proper error checking and exit codes

## Expected Output

Successful test output looks like:
```
========================================
SmartPoi WiFi and HTTP Test Script
========================================
[TEST] Step 1: Connecting to SmartPoi WiFi
[TEST] Connecting to WiFi SSID: Smart_Poi9
[SUCCESS] Connected to Smart_Poi9
[TEST] Step 2: Testing HTTP connectivity
[SUCCESS] HTTP server is responding (Status: 200)
[TEST] Step 3: Listing files
[SUCCESS] File list retrieved successfully
[TEST] Found .bin files
[TEST] First .bin file found: /a.bin
[TEST] Step 4: Deleting file
[SUCCESS] DELETE request sent successfully
[SUCCESS] File deletion appears successful
[TEST] Step 5: Verifying deletion
[SUCCESS] File /a.bin successfully deleted
========================================
[SUCCESS] All tests completed successfully!
========================================
```