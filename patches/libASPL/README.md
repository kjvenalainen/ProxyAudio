# libASPL patches

These patches are applied by [`../apply-patches.sh`](../apply-patches.sh) when ProxyAudio builds libASPL. See the [parent patch guide](../README.md) for the shared workflow.

## `fix-volumecontrol-incomplete-type.patch`

**Why it exists:** `aspl::VolumeControl` owns a `std::unique_ptr<VolumeCurve>` while `VolumeCurve` is forward-declared in the header. Some compilation paths instantiate the inline destructor before `VolumeCurve` is complete, causing an incomplete-type error.

**What it changes:** the patch declares `VolumeControl`'s destructor in `include/aspl/VolumeControl.hpp` and defines it in `src/VolumeControl.cpp`, where `VolumeCurve` is complete.

**Check it:**

```bash
cd libASPL
git apply --check ../patches/libASPL/fix-volumecontrol-incomplete-type.patch
```

If the reverse check succeeds instead, the patch is already present:

```bash
git apply --check --reverse ../patches/libASPL/fix-volumecontrol-incomplete-type.patch
```
