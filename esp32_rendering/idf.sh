#!/bin/bash
# ESP-IDF Environment Wrapper for RLCD Project
# Requires: ESP-IDF 5.5.x installed via ESP-IDF Extension Manager or manual install
#
# Usage:
#   ./idf.sh build          # Build the project
#   ./idf.sh flash          # Flash to connected device
#   ./idf.sh monitor        # Open serial monitor
#   ./idf.sh flash monitor  # Flash and monitor
#   ./idf.sh menuconfig     # Configure project
#   ./idf.sh clean          # Full clean
#   ./idf.sh <any idf.py command>

set -e

REQUIRED_IDF_VERSION="5.5"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

error() { echo -e "${RED}ERROR: $1${NC}" >&2; exit 1; }
warn() { echo -e "${YELLOW}WARNING: $1${NC}" >&2; }
info() { echo -e "${GREEN}$1${NC}"; }

# Find ESP-IDF installation
find_idf() {
    local idf_paths=(
        # ESP-IDF Extension Manager (EIM) locations
        "$HOME/.espressif/v${REQUIRED_IDF_VERSION}"*/esp-idf
        "$HOME/.espressif/frameworks/esp-idf-v${REQUIRED_IDF_VERSION}"*
        # Standard locations
        "$HOME/esp/esp-idf"
        "/opt/esp-idf"
        "$ESP_IDF"
        "$IDF_PATH"
    )

    for path_pattern in "${idf_paths[@]}"; do
        # Expand glob patterns
        for path in $path_pattern; do
            if [[ -f "$path/tools/idf.py" ]]; then
                # Check version
                local version_file="$path/version.txt"
                if [[ -f "$version_file" ]]; then
                    local version=$(cat "$version_file")
                    if [[ "$version" == v${REQUIRED_IDF_VERSION}* ]]; then
                        echo "$path"
                        return 0
                    fi
                fi
                # Try cmake version
                local cmake_version="$path/tools/cmake/version.cmake"
                if [[ -f "$cmake_version" ]]; then
                    local major=$(grep 'IDF_VERSION_MAJOR' "$cmake_version" | grep -o '[0-9]*')
                    local minor=$(grep 'IDF_VERSION_MINOR' "$cmake_version" | grep -o '[0-9]*')
                    if [[ "$major.$minor" == "$REQUIRED_IDF_VERSION" ]]; then
                        echo "$path"
                        return 0
                    fi
                fi
            fi
        done
    done

    return 1
}

# Find Python venv for ESP-IDF
find_python_env() {
    local idf_path="$1"
    local python_paths=(
        "$HOME/.espressif/tools/python/v${REQUIRED_IDF_VERSION}"*/venv
        "$HOME/.espressif/python_env/idf${REQUIRED_IDF_VERSION}"*
        "$idf_path/.venv"
    )

    for path_pattern in "${python_paths[@]}"; do
        for path in $path_pattern; do
            if [[ -f "$path/bin/python" ]]; then
                echo "$path"
                return 0
            fi
        done
    done

    return 1
}

# Find toolchain
find_toolchain() {
    local toolchain_paths=(
        "$HOME/.espressif/tools/xtensa-esp-elf"/esp-*/xtensa-esp-elf/bin
        "$HOME/.espressif/tools/xtensa-esp32s3-elf"/*/xtensa-esp32s3-elf/bin
    )

    for path_pattern in "${toolchain_paths[@]}"; do
        for path in $path_pattern; do
            if [[ -x "$path/xtensa-esp32s3-elf-gcc" ]]; then
                echo "$path"
                return 0
            fi
        done
    done

    return 1
}

# Main setup
main() {
    # Find IDF
    IDF_PATH=$(find_idf) || error "ESP-IDF ${REQUIRED_IDF_VERSION}.x not found!

Please install ESP-IDF using one of these methods:

1. ESP-IDF Extension Manager (Recommended):
   - Install VS Code ESP-IDF extension
   - Or download from: https://github.com/espressif/idf-installer

2. Manual installation:
   mkdir -p ~/esp && cd ~/esp
   git clone -b v${REQUIRED_IDF_VERSION} --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf && ./install.sh esp32s3

Then run this script again."

    info "Found ESP-IDF at: $IDF_PATH"
    export IDF_PATH

    # Find Python environment
    IDF_PYTHON_ENV_PATH=$(find_python_env "$IDF_PATH") || error "Python venv for ESP-IDF not found!

Run: cd $IDF_PATH && ./install.sh esp32s3"

    export IDF_PYTHON_ENV_PATH
    info "Found Python env at: $IDF_PYTHON_ENV_PATH"

    # Find toolchain
    TOOLCHAIN_PATH=$(find_toolchain) || error "Xtensa toolchain not found!

Run: cd $IDF_PATH && ./install.sh esp32s3"

    info "Found toolchain at: $TOOLCHAIN_PATH"

    # Set up environment
    export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$HOME/.espressif/tools}"
    export PATH="$TOOLCHAIN_PATH:$IDF_PYTHON_ENV_PATH/bin:$PATH"

    # Optional: ESP ROM ELF for debugging
    if [[ -d "$HOME/.espressif/tools/esp-rom-elfs" ]]; then
        export ESP_ROM_ELF_DIR=$(ls -d "$HOME/.espressif/tools/esp-rom-elfs"/*/ 2>/dev/null | head -1)
    fi

    # Change to project directory
    cd "$(dirname "$0")"

    # Run idf.py with all arguments
    if [[ $# -eq 0 ]]; then
        info "Usage: $0 <idf.py command> [args...]"
        info "Examples:"
        info "  $0 build"
        info "  $0 flash"
        info "  $0 flash monitor"
        info "  $0 menuconfig"
        python "$IDF_PATH/tools/idf.py" --help
    else
        info "Running: idf.py $*"
        python "$IDF_PATH/tools/idf.py" "$@"
    fi
}

main "$@"
