#!/bin/bash

# Script to install ProxyAudio driver from the latest build
# This script validates that build products exist at build/latest and installs them

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: Build directory does not exist: $BUILD_DIR"
    echo "Please build the project first."
    exit 1
fi

# Check if latest symlink exists
LATEST_SYMLINK="$BUILD_DIR/latest"
if [[ ! -L "$LATEST_SYMLINK" ]]; then
    echo "Error: Latest build symlink does not exist: $LATEST_SYMLINK"
    echo "Please run writeLatestBuildLink.sh first to create the symlink."
    exit 1
fi

# Check if the symlink points to a valid directory
if [[ ! -d "$LATEST_SYMLINK" ]]; then
    echo "Error: Latest build symlink points to non-existent directory: $LATEST_SYMLINK"
    echo "Please rebuild the project and run writeLatestBuildLink.sh again."
    exit 1
fi

# Validate that the driver bundle exists
DRIVER_BUNDLE="$LATEST_SYMLINK"
if [[ ! -d "$DRIVER_BUNDLE/Contents" ]]; then
    echo "Error: Invalid driver bundle structure at: $DRIVER_BUNDLE"
    echo "Expected to find Contents directory inside the driver bundle."
    exit 1
fi

# Check if Info.plist exists
if [[ ! -f "$DRIVER_BUNDLE/Contents/Info.plist" ]]; then
    echo "Error: Missing Info.plist in driver bundle: $DRIVER_BUNDLE/Contents/Info.plist"
    exit 1
fi

# Get the actual target directory that the symlink points to
ACTUAL_TARGET=$(readlink "$LATEST_SYMLINK")
FULL_TARGET_PATH="$BUILD_DIR/$ACTUAL_TARGET"

echo "Installing ProxyAudio driver..."
echo "Source: $FULL_TARGET_PATH"
echo "Target: /Library/Audio/Plug-Ins/HAL/"

# Perform the installation
echo "Copying driver bundle..."
sudo cp -R "$FULL_TARGET_PATH" /Library/Audio/Plug-Ins/HAL/

echo "Restarting Core Audio daemon..."
sudo killall -9 coreaudiod

echo "ProxyAudio driver installed successfully!"
echo "The driver from build/$ACTUAL_TARGET has been installed to /Library/Audio/Plug-Ins/HAL/"
