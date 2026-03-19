#!/bin/bash

# Binary creation script for SmartPoi_ESP32
# This script compiles firmware for both main and auxiliary configurations,
# and creates a littlefs.bin file, copying them to a release folder.

set -e  # Exit on any error

# Get current date for release folder name
RELEASE_DATE=$(date +'%Y-%m-%d')
RELEASE_DIR="releases/release-${RELEASE_DATE}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required commands
check_requirements() {
    local missing=0
    
    if ! command_exists pio; then
        print_error "PlatformIO CLI (pio) not found. Please install PlatformIO."
        missing=1
    fi
    
    if [ $missing -eq 1 ]; then
        exit 1
    fi
}

# Function to update auxillary flag in platformio.ini
update_auxillary_flag() {
    local value="$1"
    
    print_info "Updating auxillary flag to ${value} in platformio.ini..."
    
    # Find the line with auxillary flag for other_c3_board environment
    # We need to be careful to only update the correct environment
    # The line has leading whitespace (tab) and may have varying spacing
    sed -i "/^\\[env:other_c3_board\\]/,/^\\[/ s/.*-D auxillary=.*$/\t-D auxillary=${value} ; false for main poi, true for auxillary. Auxillary do not work alone./" platformio.ini
    
    # Verify the change
    if grep -q "auxillary=${value}" platformio.ini; then
        print_info "Successfully updated auxillary flag to ${value}"
    else
        print_error "Failed to update auxillary flag to ${value}"
        exit 1
    fi
}

# Function to update NUMPX, NUMLEDS, and MAXPX values in platformio.ini
update_size_params() {
    local numpx="$1"
    local numleds="$2"
    local maxpx="$3"
    
    print_info "Updating size parameters: NUMPX=${numpx}, NUMLEDS=${numleds}, MAXPX=${maxpx}"
    
    # Update NUMPX
    sed -i "/^\\[env:other_c3_board\\]/,/^\\[/ s/.*-D NUMPX=.*$/\t-D NUMPX=${numpx} ; number of LED's/" platformio.ini
    
    # Update NUMLEDS
    sed -i "/^\\[env:other_c3_board\\]/,/^\\[/ s/.*-D NUMLEDS=.*$/\t-D NUMLEDS=${numleds} ; number of LED's plus one - needed for some WS2812 strips/" platformio.ini
    
    # Update MAXPX
    sed -i "/^\\[env:other_c3_board\\]/,/^\\[/ s/.*-D MAXPX=.*$/\t-D MAXPX=${maxpx} ; calculate according to size of image wanted. Too large means out of memory issues./" platformio.ini
    
    # Verify the changes
    if grep -q "NUMPX=${numpx}" platformio.ini && grep -q "NUMLEDS=${numleds}" platformio.ini && grep -q "MAXPX=${maxpx}" platformio.ini; then
        print_info "Successfully updated size parameters"
    else
        print_error "Failed to update size parameters"
        exit 1
    fi
}

# Function to compile firmware
compile_firmware() {
    local env_name="$1"
    local output_file="$2"
    
    print_info "Compiling firmware for ${env_name}..."
    
    # Run platformio build
    if pio run -e "${env_name}"; then
        # Find the firmware.bin file
        local firmware_path=".pio/build/${env_name}/firmware.bin"
        
        if [ -f "${firmware_path}" ]; then
            cp "${firmware_path}" "${output_file}"
            print_info "Firmware compiled successfully: ${output_file}"
            # Show file size
            local size=$(stat -c%s "${output_file}")
            print_info "File size: $((${size}/1024)) KB"
        else
            print_error "Firmware file not found at ${firmware_path}"
            exit 1
        fi
    else
        print_error "Compilation failed for ${env_name}"
        exit 1
    fi
}

# Function to compile littlefs filesystem
compile_littlefs() {
    local env_name="$1"
    local output_file="$2"
    
    print_info "Compiling LittleFS filesystem for ${env_name}..."
    
    # Run platformio build filesystem
    if pio run -e "${env_name}" -t buildfs; then
        # Find the littlefs.bin file
        local littlefs_path=".pio/build/${env_name}/littlefs.bin"
        
        if [ -f "${littlefs_path}" ]; then
            cp "${littlefs_path}" "${output_file}"
            print_info "LittleFS compiled successfully: ${output_file}"
            # Show file size
            local size=$(stat -c%s "${output_file}")
            print_info "File size: $((${size}/1024)) KB"
        else
            print_error "LittleFS file not found at ${littlefs_path}"
            exit 1
        fi
    else
        print_error "LittleFS compilation failed for ${env_name}"
        exit 1
    fi
}

# Main script execution
main() {
    print_info "Starting binary creation for date: ${RELEASE_DATE}"
    
    # Check requirements
    check_requirements
    
    # Check if release directory already exists
    if [ -d "${RELEASE_DIR}" ]; then
        print_warn "Release directory ${RELEASE_DIR} already exists"
        read -p "Do you want to continue and overwrite? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Aborting..."
            exit 0
        fi
        # Clean up existing release directory
        print_warn "Removing existing release directory: ${RELEASE_DIR}"
        rm -rf "${RELEASE_DIR}"
    fi
    
    print_info "Creating release directory: ${RELEASE_DIR}"
    mkdir -p "${RELEASE_DIR}"
    
    # Save original platformio.ini state
    print_info "Backing up original platformio.ini"
    cp platformio.ini platformio.ini.backup
    
    # Define sizes and their corresponding MAXPX values
    # Format: NUMPX:NUMLEDS:MAXPX
    declare -A sizes=(
        ["36"]="37:9360"    # NUMLEDS=NUMPX+1=37, MAXPX=9360
        ["60"]="61:12000"   # NUMLEDS=NUMPX+1=61, MAXPX=12000
        ["72"]="73:12096"   # NUMLEDS=NUMPX+1=73, MAXPX=12096
        ["120"]="121:14400" # NUMLEDS=NUMPX+1=121, MAXPX=14400
    )
    
    # Compile LittleFS once (same for all sizes)
    print_info "Compiling LittleFS filesystem (same for all sizes)..."
    compile_littlefs "other_c3_board" "${RELEASE_DIR}/littlefs.bin"
    
    # Loop through each size
    for numpx in "${!sizes[@]}"; do
        IFS=':' read -r numleds maxpx <<< "${sizes[$numpx]}"
        
        print_info "\n=== Processing size: ${numpx} LEDs ==="
        
        # Update size parameters
        update_size_params "${numpx}" "${numleds}" "${maxpx}"
        
        # 1. Compile auxiliary firmware
        update_auxillary_flag "true"
        compile_firmware "other_c3_board" "${RELEASE_DIR}/firmware_auxiliary_${numpx}.bin"
        
        # 2. Compile main firmware
        update_auxillary_flag "false"
        compile_firmware "other_c3_board" "${RELEASE_DIR}/firmware_main_${numpx}.bin"
    done
    
    # Restore original platformio.ini
    print_info "Restoring original platformio.ini"
    mv platformio.ini.backup platformio.ini
    
    print_info "\nBinary creation completed successfully!"
    print_info "Files created in: ${RELEASE_DIR}"
    print_info "Files created:"
    ls -lh "${RELEASE_DIR}"
    
    # Summary
    print_info "\n=== SUMMARY ==="
    print_info "Generated 8 firmware files (main+aux for each size):"
    print_info "  - 36 LEDs: firmware_main_36.bin, firmware_auxiliary_36.bin"
    print_info "  - 60 LEDs: firmware_main_60.bin, firmware_auxiliary_60.bin"
    print_info "  - 72 LEDs: firmware_main_72.bin, firmware_auxiliary_72.bin"
    print_info "  - 120 LEDs: firmware_main_120.bin, firmware_auxiliary_120.bin"
    print_info "Generated 1 LittleFS file: littlefs.bin"
}

# Run main function
main "$@"