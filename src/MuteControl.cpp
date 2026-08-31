// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "MuteControl.hpp"

#include <memory>

#include "Dispatch.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

MuteControl::MuteControl(std::shared_ptr<const aspl::Context> context,
                         const MuteControlParameters& params)
    : aspl::MuteControl(context, params.AsAsplParameters()),
      storage_(std::const_pointer_cast<aspl::Context>(context)),
      storageKey_(params.StorageKey) {
  // If a storage key is provided, read the value from storage and set it.
  if (!storageKey_.empty()) {
    const auto [value, success] = storage_.ReadBoolean(storageKey_);
    if (success) {
      aspl::MuteControl::SetIsMutedImpl(value);
    }
  }
}

OSStatus MuteControl::SetIsMutedImpl(bool value) {
  const auto status = aspl::MuteControl::SetIsMutedImpl(value);

  // If the previous value was invalid, we need to set up a timer to write the
  // value to storage. Otherwise, there's already a pending timer and we don't
  // need to do anything here.
  const auto setValue = GetIsMuted() ? 1 : 0;
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
                    "MuteControl:SetIsMutedImpl() Invalid value to write to "
                    "storage");

            return;
          }

          const auto result = storage_.WriteBoolean(storageKey_, valueToWrite);
          if (!result) {
            ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
                ->Message(ProxyAudio::Tracer::Error,
                          "MuteControl:SetIsMutedImpl() Failed to write "
                          "value to storage");
          }
        },
        DispatchTime::Seconds(0.5));
  }

  return status;
}

}  // namespace ProxyAudio
