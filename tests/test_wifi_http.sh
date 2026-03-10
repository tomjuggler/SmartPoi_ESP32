#!/bin/bash

# Test script for SmartPoi WiFi connectivity and HTTP requests
# This script connects to the SmartPoi AP and tests file operations

# Configuration
SSID="Smart_Poi9"
PASSWORD="SmartOne"
POI_IP="192.168.1.1"  # Default AP IP from main.cpp
PORT=80

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${YELLOW}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to connect to WiFi using nmcli
connect_to_wifi() {
    print_status "Connecting to WiFi SSID: $SSID"
    
    if ! command_exists nmcli; then
        print_error "nmcli not found. Please install NetworkManager."
        return 1
    fi
    
    # Check if already connected to this network
    CURRENT_CONNECTION=$(nmcli -t -f NAME connection show --active)
    if echo "$CURRENT_CONNECTION" | grep -q "$SSID"; then
        print_status "Already connected to $SSID"
        return 0
    fi
    
    # Get the WiFi interface name
    WIFI_INTERFACE=$(nmcli -t -f DEVICE,TYPE device status | grep ':wifi$' | cut -d: -f1 | head -1)
    
    if [ -z "$WIFI_INTERFACE" ]; then
        print_error "No WiFi interface found"
        return 1
    fi
    
    print_status "Found WiFi interface: $WIFI_INTERFACE"
    
    # Note: We don't disconnect from current network - nmcli will handle switching
    
    # Try to connect using the simple method first
    print_status "Running: nmcli device wifi connect \"$SSID\" password \"$PASSWORD\""
    CONNECT_OUTPUT=$(nmcli device wifi connect "$SSID" password "$PASSWORD" 2>&1)
    CONNECT_EXIT_CODE=$?
    
    # If that fails, try creating a temporary connection profile with explicit key management
    if [ $CONNECT_EXIT_CODE -ne 0 ]; then
        print_status "Simple connection failed, trying with explicit key management..."
        
        # Create a temporary connection profile with fixed name
        TEMP_CONNECTION="SmartPoi_Temp"
        
        # First, delete any existing connection with this name
        nmcli connection delete "$TEMP_CONNECTION" 2>/dev/null
        
        # Create new connection with explicit key management
        print_status "Creating temporary connection: $TEMP_CONNECTION"
        CONNECT_OUTPUT=$(nmcli connection add type wifi con-name "$TEMP_CONNECTION" \
            ifname "$WIFI_INTERFACE" ssid "$SSID" \
            wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$PASSWORD" 2>&1)
        
        if [ $? -eq 0 ]; then
            # Activate the connection
            print_status "Activating connection: $TEMP_CONNECTION"
            CONNECT_OUTPUT=$(nmcli connection up "$TEMP_CONNECTION" 2>&1)
            CONNECT_EXIT_CODE=$?
            
            # Clean up the temporary connection (whether it succeeded or failed)
            nmcli connection delete "$TEMP_CONNECTION" 2>/dev/null
        else
            CONNECT_EXIT_CODE=1
        fi
    fi
    
    if [ $CONNECT_EXIT_CODE -eq 0 ]; then
        print_success "Connected to $SSID"
        return 0
    else
        print_error "Failed to connect to $SSID"
        echo "Output: $CONNECT_OUTPUT"
        return 1
    fi
}

# Function to test HTTP connectivity
test_http_connectivity() {
    print_status "Testing HTTP connectivity to $POI_IP:$PORT"
    
    # Try to get the main page
    RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 10 "http://$POI_IP:$PORT/" 2>&1)
    CURL_EXIT_CODE=$?
    
    if [ $CURL_EXIT_CODE -eq 0 ]; then
        print_success "HTTP server is responding (Status: $RESPONSE)"
        return 0
    else
        print_error "HTTP server not responding"
        echo "Curl output: $RESPONSE"
        return 1
    fi
}

# Function to list files on the SmartPoi
list_files() {
    print_status "Listing files on SmartPoi..."
    
    RESPONSE=$(curl -s --connect-timeout 10 "http://$POI_IP:$PORT/list" 2>&1)
    CURL_EXIT_CODE=$?
    
    if [ $CURL_EXIT_CODE -eq 0 ]; then
        print_success "File list retrieved successfully"
        echo "Files: $RESPONSE"
        
        # Check if there are any .bin files
        if echo "$RESPONSE" | grep -q '\.bin'; then
            print_status "Found .bin files"
            # Extract first .bin file name
            BIN_FILE=$(echo "$RESPONSE" | grep -o '"[^"]*\.bin"' | head -1 | tr -d '"')
            if [ -n "$BIN_FILE" ]; then
                print_status "First .bin file found: $BIN_FILE"
                echo "$BIN_FILE" > /tmp/smartpoi_test_bin_file.txt
                return 0
            fi
        else
            print_status "No .bin files found"
            return 1
        fi
    else
        print_error "Failed to list files"
        echo "Curl output: $RESPONSE"
        return 1
    fi
}

# Function to delete a file
delete_file() {
    local FILE_PATH="$1"
    
    if [ -z "$FILE_PATH" ]; then
        print_error "No file path provided for deletion"
        return 1
    fi
    
    print_status "Attempting to delete file: $FILE_PATH"
    
    # First send OPTIONS request for CORS preflight
    OPTIONS_RESPONSE=$(curl -s -X OPTIONS --connect-timeout 10 \
        -H "Access-Control-Request-Method: DELETE" \
        -H "Origin: http://localhost" \
        "http://$POI_IP:$PORT/edit" 2>&1)
    
    # Now send DELETE request
    DELETE_RESPONSE=$(curl -s -X DELETE --connect-timeout 10 \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "path=$FILE_PATH" \
        "http://$POI_IP:$PORT/edit" 2>&1)
    CURL_EXIT_CODE=$?
    
    if [ $CURL_EXIT_CODE -eq 0 ]; then
        print_success "DELETE request sent successfully"
        echo "Response: $DELETE_RESPONSE"
        
        # Check if deletion was successful
        if echo "$DELETE_RESPONSE" | grep -q -i "deleted\|success"; then
            print_success "File deletion appears successful"
            return 0
        else
            print_status "DELETE response: $DELETE_RESPONSE"
            return 1
        fi
    else
        print_error "DELETE request failed"
        echo "Curl output: $DELETE_RESPONSE"
        return 1
    fi
}

# Function to verify file was deleted
verify_deletion() {
    local FILE_PATH="$1"
    
    print_status "Verifying deletion of $FILE_PATH..."
    
    # List files again
    RESPONSE=$(curl -s --connect-timeout 10 "http://$POI_IP:$PORT/list" 2>&1)
    
    if echo "$RESPONSE" | grep -q "$FILE_PATH"; then
        print_error "File $FILE_PATH still exists"
        return 1
    else
        print_success "File $FILE_PATH successfully deleted"
        return 0
    fi
}

# Main test sequence
main() {
    echo "========================================"
    echo "SmartPoi WiFi and HTTP Test Script"
    echo "========================================"
    
    # Step 1: Connect to WiFi
    print_status "Step 1: Connecting to SmartPoi WiFi"
    if ! connect_to_wifi; then
        print_error "Failed to connect to WiFi. Exiting."
        exit 1
    fi
    
    # Give some time for connection to stabilize
    print_status "Waiting 5 seconds for connection to stabilize..."
    sleep 5
    
    # Step 2: Test HTTP connectivity
    print_status "Step 2: Testing HTTP connectivity"
    if ! test_http_connectivity; then
        print_error "HTTP connectivity test failed. Exiting."
        exit 1
    fi
    
    # Step 3: List files
    print_status "Step 3: Listing files"
    if ! list_files; then
        print_error "No .bin files found to test deletion. Exiting."
        exit 1
    fi
    
    # Get the .bin file to delete
    if [ -f /tmp/smartpoi_test_bin_file.txt ]; then
        BIN_FILE_TO_DELETE=$(cat /tmp/smartpoi_test_bin_file.txt)
        print_status "Will attempt to delete: $BIN_FILE_TO_DELETE"
        
        # Ask for confirmation (safety check)
        echo ""
        read -p "Do you want to delete file '$BIN_FILE_TO_DELETE'? (y/N): " -n 1 -r
        echo ""
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_status "Deletion cancelled by user"
            exit 0
        fi
        
        # Step 4: Delete the file
        print_status "Step 4: Deleting file"
        if ! delete_file "$BIN_FILE_TO_DELETE"; then
            print_error "File deletion failed"
            exit 1
        fi
        
        # Step 5: Verify deletion
        print_status "Step 5: Verifying deletion"
        if ! verify_deletion "$BIN_FILE_TO_DELETE"; then
            print_error "Deletion verification failed"
            exit 1
        fi
        
        # Clean up temp file
        rm -f /tmp/smartpoi_test_bin_file.txt
    else
        print_error "No .bin file identified for deletion"
        exit 1
    fi
    
    echo ""
    echo "========================================"
    print_success "All tests completed successfully!"
    echo "========================================"
}

# Run main function
main "$@"