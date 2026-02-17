// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "VolumeControl.hpp"

#include <MacTypes.h>

#include <memory>

#include "Dispatch.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

VolumeControl::VolumeControl(std::shared_ptr<const aspl::Context> context,
                             const VolumeControlParameters& params)
    : aspl::VolumeControl(context, params.AsAsplParameters()),
      storage_(std::const_pointer_cast<aspl::Context>(context)),
      storageKey_(params.StorageKey) {
  // If a storage key is provided, read the value from storage and set it.
  if (!storageKey_.empty()) {
    const auto [value, success] = storage_.ReadInt(storageKey_);
    if (success) {
      aspl::VolumeControl::SetRawValueImpl(static_cast<SInt32>(value));
    }
  }
}

OSStatus VolumeControl::SetRawValueImpl(SInt32 value) {
  const auto status = aspl::VolumeControl::SetRawValueImpl(value);

  // Value may have been clamped, so we need to get the actual value that was
  // set.
  const auto setValue = GetRawValue();

  // If the previous value was invalid, we need to set up a timer to write the
  // value to storage. Otherwise, there's already a pending timer and we don't
  // need to do anything here.
  if (pendingValue_.exchange(setValue, std::memory_order_acq_rel) ==
          INVALID_VALUE &&
      !storageKey_.empty()) {
    DispatchAfter(
        ^() {
          // Get the value to write to storage and clear the pending value.
          const auto valueToWrite =
              pendingValue_.exchange(INVALID_VALUE, std::memory_order_acq_rel);

          // Should never happen, but just in case.
          if (valueToWrite == INVALID_VALUE) {
            ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
                ->Message(
                    ProxyAudio::Tracer::Error,
                    "VolumeControl:SetRawValueImpl() Invalid value to write "
                    "to storage");

            return;
          }

          // Truncate the value to the range of SInt32 and write it to storage.
          const auto result =
              storage_.WriteInt(storageKey_, static_cast<SInt32>(valueToWrite));
          if (!result) {
            ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
                ->Message(ProxyAudio::Tracer::Error,
                          "VolumeControl:SetRawValueImpl() Failed to write "
                          "value to storage");
          }
        },
        DispatchTime::Seconds(0.5));
  }

  return status;
}

}  // namespace ProxyAudio
