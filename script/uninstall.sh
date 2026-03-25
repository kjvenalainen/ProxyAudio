#!/bin/bash

# Script to uninstall ProxyAudio driver from /Library/Audio/Plug-Ins/HAL/

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Resolve the latest build symlink to find the driver name
LATEST_SYMLINK="$BUILD_DIR/latest"
if [[ ! -L "$LATEST_SYMLINK" ]]; then
    echo "Error: Latest build symlink does not exist: $LATEST_SYMLINK"
    echo "Cannot determine which driver to uninstall."
    exit 1
fi

ACTUAL_TARGET=$(readlink "$LATEST_SYMLINK")
FULL_TARGET_PATH="$BUILD_DIR/$ACTUAL_TARGET"
DRIVER_NAME=$(basename "$FULL_TARGET_PATH")
INSTALLED_PATH="/Library/Audio/Plug-Ins/HAL/$DRIVER_NAME"

if [[ ! -d "$INSTALLED_PATH" ]]; then
    echo "Error: Driver not found at: $INSTALLED_PATH"
    echo "Nothing to uninstall."
    exit 1
fi

echo "Uninstalling $DRIVER_NAME..."
echo "Removing: $INSTALLED_PATH"

sudo rm -rf "$INSTALLED_PATH"

echo "Restarting Core Audio daemon..."
sudo killall -9 coreaudiod

echo "$DRIVER_NAME uninstalled successfully from /Library/Audio/Plug-Ins/HAL/"
