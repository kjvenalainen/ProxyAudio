# ProxyAudio Patch System

This directory contains patches that are automatically applied to git submodules during the build process.

## Overview

The patch system allows you to maintain local modifications to submodules (like libASPL) without forking them. Patches are automatically applied during the CMake configuration phase.

## How It Works

1. **Patch Storage**: Patches are stored in subdirectories named after the submodule (e.g., `libASPL/`)
2. **Automatic Application**: The `apply-patches.sh` script is called by CMake's `ExternalProject_Add` PATCH_COMMAND
3. **Smart Detection**: The script checks if patches are already applied to avoid errors on rebuild

## Directory Structure

```
patches/
├── README.md                    # This file
├── apply-patches.sh             # Main patch application script
└── libASPL/
    ├── README.md                # libASPL-specific patch documentation
    └── *.patch                  # Patch files for libASPL
```

## Creating New Patches

### For libASPL

1. Navigate to the submodule and make your changes:
   ```bash
   cd libASPL
   # Make your code changes
   ```

2. Generate a patch file:
   ```bash
   git diff HEAD > ../patches/libASPL/descriptive-name.patch
   ```

3. Revert your changes (patches will be applied automatically during build):
   ```bash
   git checkout .
   ```

4. Document the patch in `patches/libASPL/README.md`

### For Other Submodules

Create a new subdirectory under `patches/` named after the submodule and follow the same process.

## Testing Patches

### Test if a patch applies cleanly:
```bash
cd libASPL
git apply --check ../patches/libASPL/patch-name.patch
```

### Manually apply a patch:
```bash
cd libASPL
git apply ../patches/libASPL/patch-name.patch
```

### Manually revert changes:
```bash
cd libASPL
git checkout .
```

## Updating Patches

When a submodule is updated, patches may need to be regenerated:

1. Update the submodule:
   ```bash
   cd libASPL
   git pull origin main
   cd ..
   git add libASPL
   git commit -m "Update libASPL submodule"
   ```

2. Try building to see if patches still apply:
   ```bash
   make all
   ```

3. If patches fail, regenerate them:
   ```bash
   cd libASPL
   # Manually apply your changes again
   git diff HEAD > ../patches/libASPL/patch-name.patch
   git checkout .
   ```

## Build Process Integration

The patch system is integrated into `src/CMakeLists.txt`:

```cmake
ExternalProject_Add(${LIBASPL_TARGET}
  ...
  PATCH_COMMAND ${PATCHES_DIR}/apply-patches.sh ${LIBASPL_SOURCE_DIR} ${PATCHES_DIR}/libASPL
  ...
)
```

The `apply-patches.sh` script:
- Checks each patch file
- Applies patches that aren't already applied
- Skips patches that are already applied
- Warns about patches that can't be applied

## Troubleshooting

### Patch fails to apply after submodule update
The submodule code has changed and the patch needs to be regenerated. See "Updating Patches" above.

### Build fails with "patch cannot be applied"
1. Check if the submodule has uncommitted changes: `cd libASPL && git status`
2. If there are changes, either commit them or revert them
3. Regenerate the patch if needed

### Want to contribute patches upstream
1. Create a proper git commit in the submodule
2. Push to your fork of the submodule
3. Create a pull request to the upstream repository
4. Once merged, remove the patch file and update the submodule reference

## Best Practices

1. **Keep patches minimal**: Only patch what's necessary
2. **Document patches**: Always update the README when adding patches
3. **Descriptive names**: Use clear, descriptive names for patch files
4. **Contribute upstream**: If possible, contribute fixes back to the original project
5. **Regular updates**: Periodically check if patches are still needed after submodule updates
