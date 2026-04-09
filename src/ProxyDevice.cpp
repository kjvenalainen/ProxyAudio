// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyDevice.hpp"

#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CFBundle.h>
#include <MacTypes.h>
#include <mach/mach_time.h>

#include <cstdint>
#include <cstring>
#include <memory>

#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Error.hpp"
#include "ProxyMuteControl.hpp"
#include "ProxyStream.hpp"
#include "ProxyVolumeControl.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"
#include "VolumeControl.hpp"
#include "aspl/VolumeControl.hpp"

namespace ProxyAudio {

// Number of audio frames the ring buffer can hold. This sets the maximum
// latency budget between the proxy device's I/O cycle and the target device's
// I/O cycle. At 44100 Hz this is ~93 ms; at 48000 Hz ~85 ms.
static constexpr UInt32 kRingBufferFrameCount = 1024;

// Target fill ratio for the ring buffer. The adaptive clock keeps the buffer
// at this level by steering the HAL's write rate.
static constexpr Float64 kTargetFillRatio = 0.5;

// Gain for the integrating clock controller. Each GetZeroTimeStampImpl call
// shifts the host time by  (fillError * gain * hostTicksPerFrame)  ticks.
// Higher values correct drift faster but risk overshoot / oscillation;
// lower values are smoother but take longer to converge.
static constexpr Float64 kClockAdjustGain = 0.5;

static UInt32 GetDeviceLatencySafely(const AudioObjectID targetDeviceID) {
  try {
    // First try to get the latency from the target device on output scope.
    return ProxyAudio::GetPropertyData<UInt32>(
        targetDeviceID,
        {
            .mSelector = kAudioDevicePropertyLatency,
            .mScope = kAudioObjectPropertyScopeOutput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});
  } catch (const OSStatusError& e) {
    // Swallow the error and try the next scope.
  }
  try {
    // If that fails, try to get the latency from the target device on input
    // scope.
    return ProxyAudio::GetPropertyData<UInt32>(
        targetDeviceID,
        {
            .mSelector = kAudioDevicePropertyLatency,
            .mScope = kAudioObjectPropertyScopeInput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});
  } catch (const OSStatusError& e) {
    // Swallow the error.
  }

  return 0;
}

aspl::DeviceParameters ProxyDevice::GetParameters(
    const AudioObjectID targetDeviceID,
    std::shared_ptr<const aspl::Context> context) {
  try {
    const auto latency = ProxyAudio::GetPropertyData<UInt32>(
        targetDeviceID,
        {
            .mSelector = kAudioDevicePropertyLatency,
            .mScope = kAudioObjectPropertyScopeOutput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    const auto latencyWithBuffer = latency + kRingBufferFrameCount;

    const auto deviceName = GetDeviceNameProperty(targetDeviceID);
    const auto deviceNameHash =
        std::to_string(std::hash<std::string>()(deviceName));

    aspl::DeviceParameters parameters{
        .Name = deviceName + " (Proxy)",
        .DeviceUID =
            SafeValueOr<std::string>(
                [targetDeviceID]() {
                  return GetDeviceUIDProperty(targetDeviceID);
                },
                deviceNameHash,
                ProxyAudio::Tracer::FromTracer(context->Tracer).get()) +
            PROXY_DEVICE_SUFFIX,
        .ModelUID = SafeValueOr<std::string>(
                        [targetDeviceID]() {
                          return GetDeviceModelUIDProperty(targetDeviceID);
                        },
                        deviceNameHash,
                        ProxyAudio::Tracer::FromTracer(context->Tracer).get()) +
                    PROXY_DEVICE_SUFFIX,
        .CanBeDefault = GetDeviceCanBeDefaultProperty(targetDeviceID),
        .CanBeDefaultForSystemSounds =
            GetDeviceCanBeDefaultForSystemSoundsProperty(targetDeviceID),
        .SampleRate = GetDeviceSampleRateProperty(targetDeviceID),
        .Latency = latencyWithBuffer,
    };

    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyDevice:GetParameters() Device latency: %u, with buffer: %u",
            latency, latencyWithBuffer);

    return parameters;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyDevice:GetParameters() Failed to get parameters for "
                  "device %u: "
                  "%s",
                  targetDeviceID, e.what());

    throw e;
  }
}

std::shared_ptr<ProxyDevice> ProxyDevice::Create(
    const AudioObjectID targetObjectID,
    std::shared_ptr<const aspl::Context> context) {
  try {
    auto proxyDevice =
        std::make_shared<ProxyDevice>(private_tag(), targetObjectID, context);
    proxyDevice->AddProxyStreams();
    return proxyDevice;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyDevice:Create() Failed to create proxy device: %s",
                  e.what());

    return nullptr;
  }
}

ProxyDevice::ProxyDevice(private_tag,
                         const AudioObjectID targetObjectID,
                         std::shared_ptr<const aspl::Context> context)
    : ProxyObject<ProxyDevice>(targetObjectID,
                               context,
                               GetParameters(targetObjectID, context)),
      sampleRateProxy_(
          targetObjectID,
          context,
          DeviceSampleRateAddress,
          [this](const Float64& value) {
            RequestConfigurationChange(
                [this, value]() { PerformSampleRateChange(value); });
          },
          [this]() { return this->GetNominalSampleRate(); }) {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Creating proxy for object: %u",
                targetObjectID);

  const auto targetDeviceClass = GetClassIdProperty(targetObjectID);
  if (targetDeviceClass != kAudioDeviceClassID) {
    throw OSStatusError(kAudioHardwareBadObjectError,
                        {
                            .mSelector = kAudioObjectPropertyClass,
                            .mScope = kAudioObjectPropertyScopeGlobal,
                            .mElement = kAudioObjectPropertyElementMain,
                        });
  }

  const auto availableSampleRates =
      ProxyAudio::GetPropertyData<std::vector<AudioValueRange>>(
          GetTargetObjectID(), DeviceAvailableSampleRatesAddress, {});

  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Available sample rates: %s",
                ToString(availableSampleRates).c_str());

  auto status = SetAvailableSampleRatesImpl(availableSampleRates);
  if (status != noErr) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyDevice:ProxyDevice() Failed to set available sample "
                  "rates: %s",
                  status);
  }

  status = SetNominalSampleRateImpl(sampleRateProxy_.GetValue());
  if (status != noErr) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyDevice:ProxyDevice() Failed to set nominal sample "
                  "rate: %s",
                  status);
  }

  RegisterCustomProperty(kAudioObjectPropertyFirmwareVersion, *this,
                         &ProxyDevice::GetVersion);
}

ProxyDevice::~ProxyDevice() {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice::~ProxyDevice() Destroying proxy for device: %u",
                GetTargetObjectID());

  // Signal both the producer and consumer to stop accessing the ring buffer.
  targetIORunning_.store(false, std::memory_order_release);

  if (ioProcID_ != nullptr) {
    // AudioDeviceStop is synchronous -- after it returns the IOProc is
    // guaranteed to no longer be executing.
    AudioDeviceStop(GetTargetObjectID(), ioProcID_);
    AudioDeviceDestroyIOProcID(GetTargetObjectID(), ioProcID_);
    ioProcID_ = nullptr;
  }

  ringBuffer_.reset();
}

CFStringRef ProxyDevice::GetVersion() const {
  // Return CFBundleShortVersionString from info.plist.
  CFStringRef version = CFBundleCopyLocalizedString(
      CFBundleGetMainBundle(), CFSTR("CFBundleVersion"), CFSTR(""), nullptr);

  return version;
}

void ProxyDevice::AddProxyStreams() {
  auto outputStreams = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
      GetTargetObjectID(),
      {
          .mSelector = kAudioDevicePropertyStreams,
          .mScope = kAudioObjectPropertyScopeOutput,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});

  auto controls = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
      GetTargetObjectID(),
      {
          .mSelector = kAudioObjectPropertyControlList,
          .mScope = kAudioObjectPropertyScopeOutput,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});

  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Cloning %lu output streams.",
                outputStreams.size());

  for (auto streamID : outputStreams) {
    auto stream = std::make_shared<ProxyStream>(
        streamID, GetContext(),
        std::static_pointer_cast<aspl::Device>(shared_from_this()));

    AddStreamAsync(stream);

    const auto volumeControlId = GetControlId(
        kAudioVolumeControlClassID, kAudioDevicePropertyScopeOutput, controls);
    if (volumeControlId != kAudioObjectUnknown) {
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ProxyDevice() Proxying volume control for "
                    "stream: %u",
                    streamID);

      auto volume =
          std::make_shared<ProxyVolumeControl>(volumeControlId, GetContext());

      AddVolumeControlAsync(volume);
      stream->AttachVolumeControl(std::move(volume));
    } else {
      // Stream does not have a volume control, so we add a virtual one.
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(
              ProxyAudio::Tracer::Info,
              "ProxyDevice:ProxyDevice() Adding virtual volume control for "
              "stream: %u",
              streamID);

      auto volume = std::make_shared<VolumeControl>(
          GetContext(), VolumeControlParameters{
                            .Scope = kAudioObjectPropertyScopeOutput,
                            .StorageKey = "VOLUME_" + GetDeviceUID() + "_" +
                                          std::to_string(streamID),

                        });
      AddVolumeControlAsync(volume);
      stream->AttachVolumeControl(std::move(volume));
    }

    const auto muteControlId = GetControlId(
        kAudioMuteControlClassID, kAudioDevicePropertyScopeOutput, controls);
    if (muteControlId != kAudioObjectUnknown) {
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ProxyDevice() Cloning mute control for "
                    "stream: %u",
                    streamID);

      auto mute =
          std::make_shared<ProxyMuteControl>(muteControlId, GetContext());
      AddMuteControlAsync(mute);
      stream->AttachMuteControl(std::move(mute));
    } else {
      // Stream does not have a mute control, so we add a virtual one.
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ProxyDevice() Adding virtual mute control for "
                    "stream: %u",
                    streamID);

      // TODO: Load last mute state from storage.
      auto mute = std::make_shared<aspl::MuteControl>(
          GetContext(), aspl::MuteControlParameters{
                            .Scope = kAudioObjectPropertyScopeOutput,
                        });
      AddMuteControlAsync(mute);
      stream->AttachMuteControl(std::move(mute));
    }
  }

  // Proxy device handles its own I/O and control requests.
  SetIOHandler(std::static_pointer_cast<ProxyDevice>(shared_from_this()));
  SetControlHandler(std::static_pointer_cast<ProxyDevice>(shared_from_this()));
}

void ProxyDevice::RemoveStreams() {
  ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:RemoveStreams() Removing streams");

  while (GetStreamCount(aspl::Direction::Input) > 0) {
    auto stream = GetStreamByIndex(aspl::Direction::Input, 0);
    if (stream) {
      RemoveStreamAsync(stream);
    }
  }

  while (GetStreamCount(aspl::Direction::Output) > 0) {
    auto stream = GetStreamByIndex(aspl::Direction::Output, 0);
    if (stream) {
      RemoveStreamAsync(stream);
    }
  }

  // Remove all volume and mute controls.
  while (GetVolumeControlCount(kAudioObjectPropertyScopeInput) > 0) {
    auto volumeControl =
        GetVolumeControlByIndex(kAudioObjectPropertyScopeInput, 0);
    if (volumeControl) {
      RemoveVolumeControlAsync(volumeControl);
    }
  }

  while (GetVolumeControlCount(kAudioObjectPropertyScopeOutput) > 0) {
    auto volumeControl =
        GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    if (volumeControl) {
      RemoveVolumeControlAsync(volumeControl);
    }
  }

  while (GetMuteControlCount(kAudioObjectPropertyScopeInput) > 0) {
    auto muteControl = GetMuteControlByIndex(kAudioObjectPropertyScopeInput, 0);
    if (muteControl) {
      RemoveMuteControlAsync(muteControl);
    }
  }

  while (GetMuteControlCount(kAudioObjectPropertyScopeOutput) > 0) {
    auto muteControl =
        GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    if (muteControl) {
      RemoveMuteControlAsync(muteControl);
    }
  }
}

// ---------------------------------------------------------------------------
// ControlRequestHandler: target device IOProc lifecycle
// ---------------------------------------------------------------------------

OSStatus ProxyDevice::OnStartIO() {
  auto tracer = ProxyAudio::Tracer::FromTracer(GetContext()->Tracer);
  tracer->Message(Tracer::Info, "ProxyDevice::OnStartIO() Starting I/O");

  // Determine the output stream format so we know the channel layout.
  auto outputStream = GetStreamByIndex(aspl::Direction::Output, 0);
  if (!outputStream) {
    tracer->Message(Tracer::Error,
                    "ProxyDevice::OnStartIO() No output stream found");
    return kAudioHardwareNotRunningError;
  }

  const auto format = outputStream->GetPhysicalFormat();
  outputChannelsPerFrame_ = format.mChannelsPerFrame;

  if (outputChannelsPerFrame_ == 0) {
    tracer->Message(
        Tracer::Error,
        "ProxyDevice::OnStartIO() Output format has 0 channels per frame");
    return kAudioHardwareNotRunningError;
  }

  // The ring buffer stores Float32 samples so that volume / mute processing
  // can operate in the float domain.  Verify the stream uses Float32.
  if (!(format.mFormatFlags & kAudioFormatFlagIsFloat) ||
      format.mBitsPerChannel != 32) {
    tracer->Message(
        Tracer::Error,
        "ProxyDevice::OnStartIO() Output format is not 32-bit float");
    return kAudioHardwareNotRunningError;
  }

  // Cache the host-tick duration of one audio frame for the adaptive clock.
  {
    struct mach_timebase_info timeBase{};
    mach_timebase_info(&timeBase);
    // hostClockFrequency = ticks-per-second
    const Float64 hostClockFrequency =
        Float64(timeBase.denom) / Float64(timeBase.numer) * 1e9;
    hostTicksPerFrame_ = hostClockFrequency / format.mSampleRate;
  }

  // Reset adaptive-clock state so that a fresh start begins with no
  // accumulated offset from a previous run.
  clockOffset_ = 0.0;

  // Create a ring buffer large enough for kRingBufferFrameCount frames.
  // Each frame has outputChannelsPerFrame_ Float32 samples.
  const size_t capacitySamples =
      static_cast<size_t>(kRingBufferFrameCount) * outputChannelsPerFrame_;
  ringBuffer_ = std::make_unique<RingBuffer<Float32>>(capacitySamples);

  // Pre-fill the buffer to 50 % capacity with silence (0.0f). This provides
  // a cushion so that the target IOProc does not immediately underrun before
  // the first OnWriteMixedOutput call arrives from the HAL.
  const size_t preFillSamples = capacitySamples / 2;
  Float32 silence[1024] = {};
  size_t remaining = preFillSamples;
  while (remaining > 0) {
    const size_t chunk = std::min(remaining, sizeof(silence) / sizeof(Float32));
    ringBuffer_->Write(silence, chunk);
    remaining -= chunk;
  }

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStartIO() Ring buffer created: "
                  "capacity=%zu samples, pre-filled=%zu samples, "
                  "channels=%u, sampleRate=%.0f",
                  capacitySamples, preFillSamples, outputChannelsPerFrame_,
                  format.mSampleRate);

  // Register an IOProc on the target hardware device.
  OSStatus status = AudioDeviceCreateIOProcID(GetTargetObjectID(), TargetIOProc,
                                              this, &ioProcID_);
  if (status != noErr) {
    tracer->Message(
        Tracer::Error,
        "ProxyDevice::OnStartIO() AudioDeviceCreateIOProcID failed: %d",
        static_cast<int>(status));
    ringBuffer_.reset();
    return status;
  }

  // Start the IOProc -- the target device will begin calling TargetIOProc.
  status = AudioDeviceStart(GetTargetObjectID(), ioProcID_);
  if (status != noErr) {
    tracer->Message(Tracer::Error,
                    "ProxyDevice::OnStartIO() AudioDeviceStart failed: %d",
                    static_cast<int>(status));
    AudioDeviceDestroyIOProcID(GetTargetObjectID(), ioProcID_);
    ioProcID_ = nullptr;
    ringBuffer_.reset();
    return status;
  }

  targetIORunning_.store(true, std::memory_order_release);

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStartIO() Target IOProc started on "
                  "device %u",
                  GetTargetObjectID());
  return kAudioHardwareNoError;
}

void ProxyDevice::OnStopIO() {
  auto tracer = ProxyAudio::Tracer::FromTracer(GetContext()->Tracer);
  tracer->Message(Tracer::Info, "ProxyDevice::OnStopIO() Stopping I/O");

  // Signal both the producer and consumer to stop accessing the ring buffer.
  targetIORunning_.store(false, std::memory_order_release);

  if (ioProcID_ != nullptr) {
    // AudioDeviceStop is synchronous -- after it returns the IOProc is
    // guaranteed to no longer be executing.
    AudioDeviceStop(GetTargetObjectID(), ioProcID_);
    AudioDeviceDestroyIOProcID(GetTargetObjectID(), ioProcID_);
    ioProcID_ = nullptr;
  }

  ringBuffer_.reset();

  // Clear adaptive-clock state.
  clockOffset_ = 0.0;
  hostTicksPerFrame_ = 0.0;

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStopIO() Target IOProc stopped");
}

// ---------------------------------------------------------------------------
// Adaptive clock: steer HAL write-rate via GetZeroTimeStamp
// ---------------------------------------------------------------------------

OSStatus ProxyDevice::GetZeroTimeStampImpl(UInt32 clientID,
                                           Float64* outSampleTime,
                                           UInt64* outHostTime,
                                           UInt64* outSeed) {
  // Let the base class compute the nominal (sample-rate-derived) timestamp.
  auto status = aspl::Device::GetZeroTimeStampImpl(clientID, outSampleTime,
                                                   outHostTime, outSeed);
  if (status != noErr) {
    return status;
  }

  // Only adjust when the ring buffer is live.
  if (!ringBuffer_ || !targetIORunning_.load(std::memory_order_relaxed)) {
    return status;
  }

  // Measure how far the fill level deviates from the target.
  //   positive error → buffer is too full  → need HAL to slow down
  //   negative error → buffer is too empty → need HAL to speed up
  const Float64 capacity = static_cast<Float64>(ringBuffer_->GetCapacity());
  const Float64 count = static_cast<Float64>(ringBuffer_->GetCount());
  const Float64 error = (count / capacity) - kTargetFillRatio;

  // Integrate the error into a cumulative clock offset (host ticks).
  // Advancing the reported host time makes the HAL think the device clock
  // is running slower than real-time, so it reduces the rate at which it
  // calls OnWriteMixedOutput.  Retarding has the opposite effect.
  clockOffset_ += error * kClockAdjustGain * hostTicksPerFrame_;

  *outHostTime += static_cast<int64_t>(clockOffset_);

  return status;
}

// ---------------------------------------------------------------------------
// IORequestHandler: buffer incoming mixed audio for the target device
// ---------------------------------------------------------------------------

void ProxyDevice::OnWriteMixedOutput(
    const std::shared_ptr<aspl::Stream>& stream,
    Float64 zeroTimestamp,
    Float64 timestamp,
    const void* bytes,
    UInt32 bytesCount) {
  if (!ringBuffer_ || !targetIORunning_.load(std::memory_order_acquire)) {
    return;
  }

  // The native format is verified as Float32 in OnStartIO, so we can
  // safely reinterpret the incoming bytes as Float32 samples.
  const auto* samples = static_cast<const Float32*>(bytes);
  const size_t sampleCount = bytesCount / sizeof(Float32);

  // Write as much as possible into the ring buffer.  The adaptive clock in
  // GetZeroTimeStampImpl steers the HAL's write rate to keep the buffer near
  // 50 % full, so overflow should be extremely rare.  If it does occur the
  // Write call returns fewer samples than requested — a harmless safety net.
  ringBuffer_->Write(samples, sampleCount);
}

// ---------------------------------------------------------------------------
// Target device IOProc callback (runs on CoreAudio realtime thread)
// ---------------------------------------------------------------------------

OSStatus ProxyDevice::TargetIOProc(AudioObjectID inDevice,
                                   const AudioTimeStamp* inNow,
                                   const AudioBufferList* inInputData,
                                   const AudioTimeStamp* inInputTime,
                                   AudioBufferList* outOutputData,
                                   const AudioTimeStamp* inOutputTime,
                                   void* inClientData) {
  auto* self = static_cast<ProxyDevice*>(inClientData);

  // If streaming has been torn down, or the ring buffer is gone, output
  // silence to avoid noise on the target device.
  if (!self->targetIORunning_.load(std::memory_order_acquire) ||
      !self->ringBuffer_) {
    for (UInt32 i = 0; i < outOutputData->mNumberBuffers; i++) {
      memset(outOutputData->mBuffers[i].mData, 0,
             outOutputData->mBuffers[i].mDataByteSize);
    }
    return noErr;
  }

  // Fill every output buffer from the ring buffer.  The adaptive clock in
  // GetZeroTimeStampImpl keeps the buffer near 50 % full, so underruns should
  // be very rare.  If one does occur the remainder is padded with silence.
  for (UInt32 i = 0; i < outOutputData->mNumberBuffers; i++) {
    auto& buf = outOutputData->mBuffers[i];
    auto* dest = static_cast<Float32*>(buf.mData);
    const UInt32 samplesNeeded = buf.mDataByteSize / sizeof(Float32);

    const size_t samplesRead = self->ringBuffer_->Read(dest, samplesNeeded);

    if (samplesRead < samplesNeeded) {
      // Underrun -- pad the rest with silence.
      memset(dest + samplesRead, 0,
             (samplesNeeded - samplesRead) * sizeof(Float32));

      static size_t log = 0;
      if (log++ % 100 == 0) {
        ProxyAudio::Tracer::FromTracer(self->GetContext()->Tracer)
            ->Message(ProxyAudio::Tracer::Info,
                      "ProxyDevice::TargetIOProc() Underrun: read %zu of %zu "
                      "samples",
                      samplesRead, samplesNeeded);
      }
    }
  }

  return noErr;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void ProxyDevice::PerformSampleRateChange(const Float64& value) {
  auto tracer = ProxyAudio::Tracer::FromTracer(GetContext()->Tracer);
  tracer->Message(ProxyAudio::Tracer::Info,
                  "ProxyDevice:PerformSampleRateChange() Device sample rate "
                  "changed to %f",
                  value);

  // This method runs inside the HAL's PerformConfigurationChange callback.
  // Exceptions MUST NOT propagate out of here — they would unwind through
  // the HAL's C code, which is undefined behaviour and typically crashes
  // the audio server.
  try {
    // Immediately stop feeding audio to the target device. The
    // samples currently in the ring buffer were produced at the
    // old sample rate and would play back at the wrong pitch on
    // the reconfigured target. Setting this flag causes the
    // IOProc to output silence and OnWriteMixedOutput to discard
    // incoming data until the HAL completes the full
    // stop → reconfigure → restart cycle.
    targetIORunning_.store(false, std::memory_order_release);

    RemoveStreams();

    SetNominalSampleRateImpl(value);

    AddProxyStreams();

    NotifyPropertiesChanged({
        kAudioDevicePropertyNominalSampleRate,
        kAudioDevicePropertyStreams,
    });
  } catch (const OSStatusError& e) {
    tracer->Message(ProxyAudio::Tracer::Error,
                    "ProxyDevice:PerformSampleRateChange() Failed during "
                    "reconfiguration: %s — device may be in a degraded state",
                    e.what());
  } catch (const std::exception& e) {
    tracer->Message(ProxyAudio::Tracer::Error,
                    "ProxyDevice:PerformSampleRateChange() Unexpected "
                    "exception: %s",
                    e.what());
  } catch (...) {
    tracer->Message(ProxyAudio::Tracer::Error,
                    "ProxyDevice:PerformSampleRateChange() Unknown exception "
                    "during reconfiguration");
  }
}

}  // namespace ProxyAudio
