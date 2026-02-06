#!/bin/bash

# Script to create a symlink to the latest build products
# Usage: ./writeLatestBuildLink.sh [Debug|Release]
# If no argument is provided, defaults to Release

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Default to Release if no argument provided
BUILD_TYPE="${1:-Release}"

# Validate build type
if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    echo "Error: Build type must be 'Debug' or 'Release'"
    echo "Usage: $0 [Debug|Release]"
    exit 1
fi

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: Build directory does not exist: $BUILD_DIR"
    exit 1
fi

# Check if the target build products exist
BUILD_PRODUCTS_DIR="$BUILD_DIR/ProxyAudio/$BUILD_TYPE/ProxyAudio.driver"
if [[ ! -d "$BUILD_PRODUCTS_DIR" ]]; then
    echo "Error: Build products do not exist: $BUILD_PRODUCTS_DIR"
    echo "Please build the project first with configuration: $BUILD_TYPE"
    exit 1
fi

# Create the symlink
SYMLINK_PATH="$BUILD_DIR/latest"

# Remove existing symlink if it exists
if [[ -L "$SYMLINK_PATH" ]]; then
    rm "$SYMLINK_PATH"
    echo "Removed existing symlink: $SYMLINK_PATH"
elif [[ -e "$SYMLINK_PATH" ]]; then
    echo "Error: $SYMLINK_PATH exists but is not a symlink"
    exit 1
fi

# Create new symlink
ln -sf "ProxyAudio/$BUILD_TYPE/ProxyAudio.driver" "$SYMLINK_PATH"

echo "Created symlink: build/latest -> build/ProxyAudio/$BUILD_TYPE/ProxyAudio.driver"
echo "Latest build products now point to: $BUILD_TYPE"
