#!/usr/bin/env bash

# Wrapper script for an xcodebuild command that invokes the build with `-gen-cdb-fragment-path`
# to generate clangd-compatible compilation database fragments and appends them to `compile_commands.json`.
#
# Example usage:
#  ./gxcWrapper.sh xcodebuild -workspace MyWorkspace.xcworkspace -scheme MyScheme build

readonly GXC_SCRIPT_DIR="$(dirname "$0")"
readonly FRAGMENT_DIR="cdb"

# Ensure that the build command does not contain `OTHER_CFLAGS`.
if grep -q 'OTHER_CFLAGS' <<< "$*"; then
  echo "OTHER_CLAGS detected in build command! This is unsupported as they will be overridden." >&2
  exit 1
fi

# Change the working directory to the repo root.
cd "$GXC_SCRIPT_DIR/.."

# Delete FRAGMENT_DIR contents.
rm -rf $FRAGMENT_DIR/*

# Invoke the wrapped compile command.
"$@" OTHER_CFLAGS="-gen-cdb-fragment-path $FRAGMENT_DIR"

# Invoke `gxcWrapper.py` to append the compilation database fragments to `compile_commands.json`.
python3 $GXC_SCRIPT_DIR/gxcWrapper.py "$FRAGMENT_DIR" compile_commands.json
