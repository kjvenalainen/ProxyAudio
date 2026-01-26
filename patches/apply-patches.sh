#!/bin/bash
# Script to apply patches to libASPL submodule
# This is called by CMake during the build process

set -e

LIBASPL_DIR="$1"
PATCHES_DIR="$2"

if [ ! -d "$LIBASPL_DIR" ]; then
    echo "Error: libASPL directory not found: $LIBASPL_DIR"
    exit 1
fi

if [ ! -d "$PATCHES_DIR" ]; then
    echo "Error: Patches directory not found: $PATCHES_DIR"
    exit 1
fi

cd "$LIBASPL_DIR"

# Apply each patch in the patches directory
for patch in "$PATCHES_DIR"/*.patch; do
    if [ -f "$patch" ]; then
        echo "Checking patch: $(basename "$patch")"
        
        # Check if patch can be applied
        if git apply --check "$patch" 2>/dev/null; then
            echo "Applying patch: $(basename "$patch")"
            git apply "$patch"
        elif git apply --check --reverse "$patch" 2>/dev/null; then
            echo "Patch already applied: $(basename "$patch")"
        else
            echo "Warning: Patch cannot be applied: $(basename "$patch")"
            echo "This might indicate the submodule has been updated and patches need regeneration"
        fi
    fi
done

echo "Patch application complete"
