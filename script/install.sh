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

# Derive the bundle name from the source (e.g. "ProxyAudio.driver")
BUNDLE_NAME="$(basename "$FULL_TARGET_PATH")"
INSTALL_DIR="/Library/Audio/Plug-Ins/HAL"
INSTALLED_BUNDLE="$INSTALL_DIR/$BUNDLE_NAME"

echo "Installing ProxyAudio driver..."
echo "Source: $FULL_TARGET_PATH"
echo "Target: $INSTALL_DIR/"

# Stop coreaudiod BEFORE touching the bundle. If we overwrite the bundle while
# coreaudiod has it loaded, the kernel keeps a cached code-signature blob
# (with the old cs_mtime) attached to the inode. cp -R truncates the existing
# file in place, reusing the inode, so the next page-in fails with:
#   "rejecting invalid page ... (cs_mtime:X != mtime:Y)"
# Killing coreaudiod first releases its vnodes so the cs_blob cache is dropped.
echo "Stopping coreaudiod..."
sudo killall -9 coreaudiod 2>/dev/null || true

# Remove the previously installed bundle so the new copy gets fresh inodes.
# Without this, cp overwrites files in place and the kernel may still have a
# stale code-signature association with the old inode.
if [[ -e "$INSTALLED_BUNDLE" ]]; then
    echo "Removing previous bundle at $INSTALLED_BUNDLE..."
    sudo rm -rf "$INSTALLED_BUNDLE"
fi

echo "Copying driver bundle..."
sudo cp -R "$FULL_TARGET_PATH" "$INSTALL_DIR/"

# launchd will restart coreaudiod automatically; nudge it once more in case
# it came back up between the rm and the cp.
echo "Restarting coreaudiod..."
sudo killall -9 coreaudiod 2>/dev/null || true

echo "ProxyAudio driver installed successfully!"
echo "The driver from build/$ACTUAL_TARGET has been installed to $INSTALL_DIR/"
