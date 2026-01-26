# libASPL Patches

This directory contains patches that are automatically applied to the libASPL submodule during the build process.

## Current Patches

### fix-volumecontrol-incomplete-type.patch
**Purpose**: Fixes incomplete type error with `VolumeCurve` in `VolumeControl`

**Issue**: The `VolumeControl` class uses `std::unique_ptr<VolumeCurve>` with only a forward declaration of `VolumeCurve`. When the compiler tries to instantiate the destructor, it needs the complete type definition.

**Solution**: Explicitly declares the destructor in the header and defines it in the .cpp file where `VolumeCurve` is fully defined.

**Files Modified**:
- `include/aspl/VolumeControl.hpp` - Added destructor declaration
- `src/VolumeControl.cpp` - Added destructor definition

## How Patches Are Applied

Patches are automatically applied during the CMake configuration phase via the `PATCH_COMMAND` in `src/CMakeLists.txt`. The command:
1. Checks if the patch can be applied cleanly
2. Applies the patch if possible
3. Continues silently if the patch is already applied or fails

## Creating New Patches

To create a new patch:

```bash
cd libASPL
# Make your changes
git diff HEAD > ../patches/libASPL/descriptive-name.patch
```

## Updating Existing Patches

If the libASPL submodule is updated and patches need to be regenerated:

```bash
cd libASPL
git checkout .  # Revert any applied patches
git pull        # Update to latest
# Make your changes again
git diff HEAD > ../patches/libASPL/patch-name.patch
```

## Testing Patches

To test if a patch applies cleanly:

```bash
cd libASPL
git apply --check ../patches/libASPL/patch-name.patch
```

To manually apply a patch:

```bash
cd libASPL
git apply ../patches/libASPL/patch-name.patch
```

To revert a patch:

```bash
cd libASPL
git checkout .
```
