#!/bin/bash

# Release creation script for SmartPoi_ESP32
# This script creates a release branch, compiles firmware for both main and auxiliary configurations,
# and creates a littlefs.bin file.

set -e  # Exit on any error

# Get current date for release folder name
RELEASE_DATE=$(date +'%Y-%m-%d')
RELEASE_BRANCH="release-${RELEASE_DATE}"
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
    
    if ! command_exists git; then
        print_error "Git not found. Please install Git."
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
    sed -i "/^\[env:other_c3_board\]/,/^\[/ s/^-D auxillary=.*$/-D auxillary=${value} ; false for main poi, true for auxillary. Auxillary do not work alone./" platformio.ini
    
    # Verify the change
    if grep -q "^-D auxillary=${value}" platformio.ini; then
        print_info "Successfully updated auxillary flag to ${value}"
    else
        print_error "Failed to update auxillary flag to ${value}"
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
    print_info "Starting release creation for date: ${RELEASE_DATE}"
    
    # Check requirements
    check_requirements
    
    # Check if we're in a git repository
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository"
        exit 1
    fi
    
    # Check if release branch already exists
    if git show-ref --verify --quiet "refs/heads/${RELEASE_BRANCH}"; then
        print_warn "Release branch ${RELEASE_BRANCH} already exists"
        read -p "Do you want to continue and overwrite? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Aborting..."
            exit 0
        fi
    fi
    
    # 1. Create new release branch
    print_info "Creating new release branch: ${RELEASE_BRANCH}"
    
    # Check if branch already exists and handle accordingly
    if git show-ref --verify --quiet "refs/heads/${RELEASE_BRANCH}"; then
        # Branch exists, check it out
        print_info "Branch already exists, checking it out..."
        git checkout "${RELEASE_BRANCH}"
        
        # Clean up any existing release directory for this date
        if [ -d "${RELEASE_DIR}" ]; then
            print_warn "Removing existing release directory: ${RELEASE_DIR}"
            rm -rf "${RELEASE_DIR}"
        fi
    else
        # Branch doesn't exist, create it
        git checkout -b "${RELEASE_BRANCH}"
    fi
    print_info "Creating release directory: ${RELEASE_DIR}"
    mkdir -p "${RELEASE_DIR}"
    
    # Save original platformio.ini state
    print_info "Backing up original platformio.ini"
    cp platformio.ini platformio.ini.backup
    
    # 2. Change auxillary=false to auxillary=true for auxiliary firmware
    update_auxillary_flag "true"
    
    # 3. Compile and save auxiliary firmware
    compile_firmware "other_c3_board" "${RELEASE_DIR}/firmware_auxiliary.bin"
    
    # 4. Change back to auxillary=false for main firmware
    update_auxillary_flag "false"
    
    # 5. Compile and save main firmware
    compile_firmware "other_c3_board" "${RELEASE_DIR}/firmware_main.bin"
    
    # 6. Compile and save littlefs.bin (using main configuration)
    compile_littlefs "other_c3_board" "${RELEASE_DIR}/littlefs.bin"
    
    # Restore original platformio.ini
    print_info "Restoring original platformio.ini"
    mv platformio.ini.backup platformio.ini
    
    # 7. Add the new files and commit
    print_info "Adding release files to git..."
    git add "${RELEASE_DIR}"
    
    print_info "Creating commit..."
    git commit -m "Release ${RELEASE_DATE}: Main and auxiliary firmware + LittleFS"
    
    print_info "Release created successfully!"
    print_info "Release branch: ${RELEASE_BRANCH}"
    print_info "Release files in: ${RELEASE_DIR}"
    print_info "Files created:"
    ls -lh "${RELEASE_DIR}"
    
    print_info "\nNext steps:"
    print_info "1. Review the compiled files"
    print_info "2. Push the branch if needed: git push origin ${RELEASE_BRANCH}"
    print_info "3. Create a release tag if desired: git tag -a v${RELEASE_DATE} -m 'Release ${RELEASE_DATE}'"
}

# Run main function
main "$@"