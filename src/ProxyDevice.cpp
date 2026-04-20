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

// Number of audio frames the ring buffer can hold. This must be meaningfully
// larger than (prebufferFrames + largest HAL write burst + one consumer cycle)
// or the buffer will oscillate between full and empty, producing simultaneous
// overruns and underruns. At 4 channels and 48 kHz, 8192 frames ≈ 170 ms.
static constexpr UInt32 kRingBufferFrameCount = 8192;

// Number of target device buffer cycles to prebuffer with silence at startup.
// This is the steady-state cushion of audio held in the ring buffer and
// dictates tolerance to scheduling jitter between producer and consumer.
// Set to 1 cycle for minimum latency; the ring buffer still has additional
// capacity to absorb occasional HAL scheduler skip/double-write events.
// 1 cycle ≈ 10.7 ms at 48 kHz with a 512-frame buffer.
static constexpr UInt32 kPrebufferCycles = 1;

// Default buffer frame size if unable to query from target device.
static constexpr UInt32 kDefaultBufferFrameSize = 256;

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

  // Reset clock forwarding state so a fresh start begins with no stale
  // anchor from a previous session.
  clockInitialized_.store(false, std::memory_order_relaxed);
  ourAnchorHostTime_.store(0, std::memory_order_relaxed);
  targetAnchorHostTime_.store(0, std::memory_order_relaxed);
  targetAnchorSampleBits_.store(0, std::memory_order_relaxed);
  targetTsSeq_.store(0, std::memory_order_relaxed);
  targetTsHostTime_.store(0, std::memory_order_relaxed);
  targetTsSampleBits_.store(0, std::memory_order_relaxed);
  zeroTimeStampPeriodCounter_.store(0, std::memory_order_relaxed);
  lastZtsHostTime_.store(0, std::memory_order_relaxed);
  underrunCount_ = 0;
  ioProcCallCount_ = 0;
  overrunSampleCount_.store(0, std::memory_order_relaxed);
  writeCallCount_.store(0, std::memory_order_relaxed);
  writeSampleCount_.store(0, std::memory_order_relaxed);
  maxWriteChunkSamples_.store(0, std::memory_order_relaxed);
  ringFillMin_ = SIZE_MAX;
  ringFillMax_ = 0;

  // Query the target device's buffer frame size to determine the prebuffer.
  UInt32 bufferFrameSize = kDefaultBufferFrameSize;
  try {
    bufferFrameSize = ProxyAudio::GetPropertyData<UInt32>(
        GetTargetObjectID(),
        {
            .mSelector = kAudioDevicePropertyBufferFrameSize,
            .mScope = kAudioObjectPropertyScopeOutput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});
  } catch (const OSStatusError& e) {
    tracer->Message(
        Tracer::Info,
        "ProxyDevice::OnStartIO() Could not query buffer frame size: %s, "
        "using default %u",
        e.what(), kDefaultBufferFrameSize);
  }

  // Prebuffer a fixed number of buffer cycles with silence to provide a
  // latency cushion while the clock filter settles.
  prebufferFrames_ = bufferFrameSize * kPrebufferCycles;

  // Cap the prebuffer at half the ring capacity so there is always enough
  // free space for the HAL's first writes; otherwise the ring starts full
  // and every OnWriteMixedOutput call drops samples until the consumer
  // catches up.
  const UInt32 maxPrebuffer = kRingBufferFrameCount / 2;
  if (prebufferFrames_ > maxPrebuffer) {
    tracer->Message(
        Tracer::Info,
        "ProxyDevice::OnStartIO() Capping prebuffer from %u to %u frames "
        "(ring capacity=%u frames)",
        prebufferFrames_, maxPrebuffer, kRingBufferFrameCount);
    prebufferFrames_ = maxPrebuffer;
  }

  // Create a ring buffer large enough for kRingBufferFrameCount frames.
  // Each frame has outputChannelsPerFrame_ Float32 samples.
  const size_t capacitySamples =
      static_cast<size_t>(kRingBufferFrameCount) * outputChannelsPerFrame_;
  ringBuffer_ = std::make_unique<RingBuffer<Float32>>(capacitySamples);

  // Pre-fill the buffer with the computed prebuffer amount.
  const size_t preFillSamples =
      static_cast<size_t>(prebufferFrames_) * outputChannelsPerFrame_;
  Float32 silence[1024] = {};
  size_t remaining = preFillSamples;
  while (remaining > 0) {
    const size_t chunk = std::min(remaining, sizeof(silence) / sizeof(Float32));
    ringBuffer_->Write(silence, chunk);
    remaining -= chunk;
  }

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStartIO() Ring buffer created: "
                  "capacity=%zu samples, pre-filled=%zu samples (%u frames), "
                  "channels=%u, sampleRate=%.0f, bufferFrameSize=%u",
                  capacitySamples, preFillSamples, prebufferFrames_,
                  outputChannelsPerFrame_, format.mSampleRate, bufferFrameSize);

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

  // Log final stats before clearing. Compute the observed mean rate over
  // the whole session directly from the anchor and latest target timestamps
  // — no filter, no state, no drift.
  Float64 observedTpf = hostTicksPerFrame_;
  if (clockInitialized_.load(std::memory_order_acquire)) {
    const UInt64 anchorHost =
        targetAnchorHostTime_.load(std::memory_order_relaxed);
    const UInt64 anchorSampleBits =
        targetAnchorSampleBits_.load(std::memory_order_relaxed);
    const UInt64 latestHost =
        targetTsHostTime_.load(std::memory_order_relaxed);
    const UInt64 latestSampleBits =
        targetTsSampleBits_.load(std::memory_order_relaxed);
    Float64 anchorSample;
    Float64 latestSample;
    std::memcpy(&anchorSample, &anchorSampleBits, sizeof(Float64));
    std::memcpy(&latestSample, &latestSampleBits, sizeof(Float64));
    const Float64 deltaSample = latestSample - anchorSample;
    if (deltaSample > 0.0 && latestHost > anchorHost) {
      observedTpf =
          static_cast<Float64>(latestHost - anchorHost) / deltaSample;
    }
  }
  const Float64 nominalTpf = hostTicksPerFrame_;
  const Float64 driftPpm =
      (nominalTpf > 0.0) ? ((observedTpf - nominalTpf) / nominalTpf) * 1.0e6
                         : 0.0;
  tracer->Message(
      Tracer::Info,
      "ProxyDevice::OnStopIO() Final stats: calls=%llu, underruns=%llu, "
      "overrunSamples=%llu, tpf=%.4f (nominal=%.4f, observed drift=%.1f ppm)",
      static_cast<unsigned long long>(ioProcCallCount_),
      static_cast<unsigned long long>(underrunCount_),
      static_cast<unsigned long long>(
          overrunSampleCount_.load(std::memory_order_relaxed)),
      observedTpf, nominalTpf, driftPpm);

  // Clear clock forwarding state.
  clockInitialized_.store(false, std::memory_order_relaxed);
  ourAnchorHostTime_.store(0, std::memory_order_relaxed);
  targetAnchorHostTime_.store(0, std::memory_order_relaxed);
  targetAnchorSampleBits_.store(0, std::memory_order_relaxed);
  targetTsSeq_.store(0, std::memory_order_relaxed);
  targetTsHostTime_.store(0, std::memory_order_relaxed);
  targetTsSampleBits_.store(0, std::memory_order_relaxed);
  zeroTimeStampPeriodCounter_.store(0, std::memory_order_relaxed);
  lastZtsHostTime_.store(0, std::memory_order_relaxed);
  hostTicksPerFrame_ = 0.0;
  prebufferFrames_ = 0;
  underrunCount_ = 0;
  ioProcCallCount_ = 0;
  overrunSampleCount_.store(0, std::memory_order_relaxed);
  writeCallCount_.store(0, std::memory_order_relaxed);
  writeSampleCount_.store(0, std::memory_order_relaxed);
  maxWriteChunkSamples_.store(0, std::memory_order_relaxed);
  ringFillMin_ = SIZE_MAX;
  ringFillMax_ = 0;

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStopIO() Target IOProc stopped");
}

// ---------------------------------------------------------------------------
// Clock forwarding: report a ZeroTimeStamp whose rate is the target device's
// ---------------------------------------------------------------------------
//
// We do not filter or reconstruct the target's clock. Instead we cache the
// (hostTime, sampleTime) pair the target HAL hands us in TargetIOProc and,
// in GetZeroTimeStampImpl, derive ticks-per-frame directly from the
// anchor-to-latest delta — a baseline that grows with the session and is
// exact by construction. We then package that rate into the same
// period-counter-driven report format aspl::Device uses so the HAL sees a
// smoothly advancing, period-aligned, monotonic device position.

OSStatus ProxyDevice::GetZeroTimeStampImpl(UInt32 clientID,
                                           Float64* outSampleTime,
                                           UInt64* outHostTime,
                                           UInt64* outSeed) {
  // Until the first TargetIOProc callback has fired, we have no target
  // timestamp to forward. Fall back to the base class, which reports
  // nominal-rate timestamps based on the device's start time.
  if (!clockInitialized_.load(std::memory_order_acquire)) {
    return aspl::Device::GetZeroTimeStampImpl(clientID, outSampleTime,
                                              outHostTime, outSeed);
  }

  const UInt64 ourAnchor = ourAnchorHostTime_.load(std::memory_order_relaxed);
  const UInt64 targetAnchorHost =
      targetAnchorHostTime_.load(std::memory_order_relaxed);
  const UInt64 targetAnchorSampleBits =
      targetAnchorSampleBits_.load(std::memory_order_relaxed);
  Float64 targetAnchorSample;
  std::memcpy(&targetAnchorSample, &targetAnchorSampleBits, sizeof(Float64));

  // Seqlock-protected read of the latest target timestamp pair.
  UInt64 seq1;
  UInt64 seq2;
  UInt64 targetHost;
  UInt64 targetSampleBits;
  do {
    seq1 = targetTsSeq_.load(std::memory_order_acquire);
    if (seq1 & 1ULL) {
      continue;
    }
    targetHost = targetTsHostTime_.load(std::memory_order_relaxed);
    targetSampleBits = targetTsSampleBits_.load(std::memory_order_relaxed);
    seq2 = targetTsSeq_.load(std::memory_order_acquire);
  } while (seq1 != seq2);

  Float64 targetSample;
  std::memcpy(&targetSample, &targetSampleBits, sizeof(Float64));

  // Derive ticks-per-frame straight from the target's own timeline. The
  // baseline (targetSample - targetAnchorSample) grows with every target
  // IOProc call, so this is mathematically the exact mean target rate over
  // the session — no filter lag, no secular bias. Until we have enough
  // baseline to trust the division (first few ms), fall back to nominal.
  const Float64 deltaSampleTarget = targetSample - targetAnchorSample;
  const UInt64 deltaHostTarget =
      (targetHost > targetAnchorHost) ? (targetHost - targetAnchorHost) : 0;
  Float64 ticksPerFrame = hostTicksPerFrame_;
  if (deltaSampleTarget > 64.0 && deltaHostTarget > 0) {
    ticksPerFrame =
        static_cast<Float64>(deltaHostTarget) / deltaSampleTarget;
  }

  const Float64 period = static_cast<Float64>(GetZeroTimeStampPeriod());
  const Float64 ticksPerPeriod = ticksPerFrame * period;

  // Advance the monotonic period counter up to (but not past) current wall
  // time. This matches aspl::Device's convention and keeps the HAL seeing
  // sampleTime progressing smoothly even between TargetIOProc updates.
  const UInt64 now = mach_absolute_time();
  UInt64 counter = zeroTimeStampPeriodCounter_.load(std::memory_order_relaxed);
  while (ticksPerPeriod > 0.0) {
    const UInt64 nextPeriodHost =
        ourAnchor +
        static_cast<UInt64>(static_cast<Float64>(counter + 1) * ticksPerPeriod);
    if (nextPeriodHost > now) {
      break;
    }
    ++counter;
  }
  zeroTimeStampPeriodCounter_.store(counter, std::memory_order_relaxed);

  // sampleTime is period-aligned plus the constant prebuffer offset so the
  // HAL's notion of device position stays a cushion's worth ahead of real
  // consumption.
  const Float64 sampleTime = static_cast<Float64>(counter) * period +
                             static_cast<Float64>(prebufferFrames_);

  // hostTime projected from the anchor at the derived target rate.
  UInt64 hostTime =
      ourAnchor + static_cast<UInt64>(sampleTime * ticksPerFrame);

  // Clamp to strict monotonicity. ticksPerFrame can shift very slightly
  // between reads (as the baseline grows), which can cause hostTime for a
  // given counter to nudge backward. The HAL rejects non-monotonic reports.
  const UInt64 lastHost = lastZtsHostTime_.load(std::memory_order_relaxed);
  if (hostTime <= lastHost) {
    hostTime = lastHost + 1;
  }
  lastZtsHostTime_.store(hostTime, std::memory_order_release);

  *outSampleTime = sampleTime;
  *outHostTime = hostTime;
  *outSeed = 1;

  return kAudioHardwareNoError;
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

  // Write as much as possible into the ring buffer. The adaptive clock in
  // GetZeroTimeStampImpl steers the HAL's write rate to match the target
  // consumption rate, so overflow should be rare. If it does occur, Write
  // returns fewer samples than requested and we track the dropped count.
  const size_t written = ringBuffer_->Write(samples, sampleCount);
  const auto callCount =
      writeCallCount_.fetch_add(1, std::memory_order_relaxed) + 1;
  writeSampleCount_.fetch_add(sampleCount, std::memory_order_relaxed);

  const auto chunk = static_cast<UInt32>(sampleCount);
  UInt32 prevMax = maxWriteChunkSamples_.load(std::memory_order_relaxed);
  while (chunk > prevMax && !maxWriteChunkSamples_.compare_exchange_weak(
                                prevMax, chunk, std::memory_order_relaxed)) {
  }

  if (written < sampleCount) {
    const size_t dropped = sampleCount - written;
    overrunSampleCount_.fetch_add(dropped, std::memory_order_relaxed);
    if (callCount % 100 == 1) {
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice::OnWriteMixedOutput() Overrun: dropped %zu "
                    "of %zu samples (ring fill=%zu/%zu)",
                    dropped, sampleCount, ringBuffer_->GetCount(),
                    ringBuffer_->GetCapacity());
    }
  }
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

  ++self->ioProcCallCount_;

  // Track ring fill extremes before we read anything, for diagnostics.
  {
    const size_t fillBefore = self->ringBuffer_->GetCount();
    if (fillBefore < self->ringFillMin_)
      self->ringFillMin_ = fillBefore;
    if (fillBefore > self->ringFillMax_)
      self->ringFillMax_ = fillBefore;
  }

  // Fill every output buffer from the ring buffer. Since the HAL writes at
  // the target's actual clock rate (we forward its timestamps verbatim), the
  // ring stays near the prebuffer cushion level and underruns should be
  // rare. If one does occur the remainder is padded with silence.
  UInt32 underrunSamplesThisCall = 0;
  for (UInt32 i = 0; i < outOutputData->mNumberBuffers; i++) {
    auto& buf = outOutputData->mBuffers[i];
    auto* dest = static_cast<Float32*>(buf.mData);
    const UInt32 samplesNeeded = buf.mDataByteSize / sizeof(Float32);

    const size_t samplesRead = self->ringBuffer_->Read(dest, samplesNeeded);

    if (samplesRead < samplesNeeded) {
      // Underrun -- pad the rest with silence.
      memset(dest + samplesRead, 0,
             (samplesNeeded - samplesRead) * sizeof(Float32));
      underrunSamplesThisCall += samplesNeeded - samplesRead;
    }
  }

  if (underrunSamplesThisCall > 0) {
    ++self->underrunCount_;
    if (self->underrunCount_ % 20 == 1) {
      ProxyAudio::Tracer::FromTracer(self->GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice::TargetIOProc() Underrun: %u samples short, "
                    "total underruns=%llu (ring fill=%zu/%zu)",
                    underrunSamplesThisCall,
                    static_cast<unsigned long long>(self->underrunCount_),
                    self->ringBuffer_->GetCount(),
                    self->ringBuffer_->GetCapacity());
    }
  }

  // Capture the target's (hostTime, sampleTime) pair and publish it to the
  // snapshot read by GetZeroTimeStampImpl. The first valid call also
  // establishes the anchor, which ties the target's clock epoch to ours so
  // we can report timestamps in our local reference frame.
  if (inOutputTime != nullptr &&
      (inOutputTime->mFlags & kAudioTimeStampHostTimeValid) &&
      (inOutputTime->mFlags & kAudioTimeStampSampleTimeValid)) {
    const UInt64 currentHostTime = inOutputTime->mHostTime;
    const Float64 currentSampleTime = inOutputTime->mSampleTime;
    UInt64 currentSampleBits;
    std::memcpy(&currentSampleBits, &currentSampleTime, sizeof(UInt64));

    if (!self->clockInitialized_.load(std::memory_order_acquire)) {
      // First valid timestamp: anchor our local clock to the target's.
      // Use mach_absolute_time() (via inNow when available) so ourAnchor
      // refers to a real present moment rather than the future time
      // inOutputTime->mHostTime points at.
      const UInt64 nowAnchor =
          (inNow != nullptr && (inNow->mFlags & kAudioTimeStampHostTimeValid))
              ? inNow->mHostTime
              : mach_absolute_time();
      self->ourAnchorHostTime_.store(nowAnchor, std::memory_order_relaxed);
      self->targetAnchorHostTime_.store(currentHostTime,
                                        std::memory_order_relaxed);
      self->targetAnchorSampleBits_.store(currentSampleBits,
                                          std::memory_order_relaxed);
      // Seed the live snapshot with the same values so the first reader
      // sees deltaSample = deltaHost = 0.
      self->targetTsHostTime_.store(currentHostTime,
                                    std::memory_order_relaxed);
      self->targetTsSampleBits_.store(currentSampleBits,
                                      std::memory_order_relaxed);
      self->targetTsSeq_.store(0, std::memory_order_relaxed);
      self->clockInitialized_.store(true, std::memory_order_release);

      ProxyAudio::Tracer::FromTracer(self->GetContext()->Tracer)
          ->Message(
              ProxyAudio::Tracer::Info,
              "ProxyDevice::TargetIOProc() Clock forwarding initialized: "
              "ourAnchorHost=%llu, targetAnchorHost=%llu, "
              "targetAnchorSample=%.0f, prebufferFrames=%u",
              static_cast<unsigned long long>(nowAnchor),
              static_cast<unsigned long long>(currentHostTime),
              currentSampleTime, self->prebufferFrames_);
    } else {
      // Seqlock-style update: bump the sequence to odd (writer busy),
      // store the new pair, bump back to even (stable). Single writer, so
      // we don't need CAS semantics.
      const UInt64 s =
          self->targetTsSeq_.load(std::memory_order_relaxed);
      self->targetTsSeq_.store(s + 1, std::memory_order_release);
      self->targetTsHostTime_.store(currentHostTime,
                                    std::memory_order_relaxed);
      self->targetTsSampleBits_.store(currentSampleBits,
                                      std::memory_order_relaxed);
      self->targetTsSeq_.store(s + 2, std::memory_order_release);
    }
  }

  // Periodically log overall streaming health. tpf is computed directly
  // from the anchor and latest target timestamps — a long baseline average
  // that converges to the exact target clock rate.
  if (self->ioProcCallCount_ % 500 == 0) {
    Float64 tpf = self->hostTicksPerFrame_;
    if (self->clockInitialized_.load(std::memory_order_acquire)) {
      const UInt64 anchorHost =
          self->targetAnchorHostTime_.load(std::memory_order_relaxed);
      const UInt64 anchorSampleBits =
          self->targetAnchorSampleBits_.load(std::memory_order_relaxed);
      const UInt64 latestHost =
          self->targetTsHostTime_.load(std::memory_order_relaxed);
      const UInt64 latestSampleBits =
          self->targetTsSampleBits_.load(std::memory_order_relaxed);
      Float64 anchorSample;
      Float64 latestSample;
      std::memcpy(&anchorSample, &anchorSampleBits, sizeof(Float64));
      std::memcpy(&latestSample, &latestSampleBits, sizeof(Float64));
      const Float64 deltaSample = latestSample - anchorSample;
      if (deltaSample > 0.0 && latestHost > anchorHost) {
        tpf = static_cast<Float64>(latestHost - anchorHost) / deltaSample;
      }
    }
    const Float64 nominal = self->hostTicksPerFrame_;
    const Float64 driftPpm =
        (nominal > 0.0) ? ((tpf - nominal) / nominal) * 1.0e6 : 0.0;
    const UInt64 overruns =
        self->overrunSampleCount_.load(std::memory_order_relaxed);
    const UInt64 writeCalls =
        self->writeCallCount_.load(std::memory_order_relaxed);
    const UInt64 writeSamples =
        self->writeSampleCount_.load(std::memory_order_relaxed);
    const UInt32 maxChunk =
        self->maxWriteChunkSamples_.load(std::memory_order_relaxed);
    const size_t fillMin = self->ringFillMin_;
    const size_t fillMax = self->ringFillMax_;
    ProxyAudio::Tracer::FromTracer(self->GetContext()->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyDevice::TargetIOProc() Stats: calls=%llu underruns=%llu "
            "overrunSamples=%llu ringFill=%zu/%zu ringMin=%zu ringMax=%zu "
            "writeCalls=%llu writeSamples=%llu maxWriteChunk=%u "
            "tpf=%.4f (nominal=%.4f observed drift=%.1f ppm)",
            static_cast<unsigned long long>(self->ioProcCallCount_),
            static_cast<unsigned long long>(self->underrunCount_),
            static_cast<unsigned long long>(overruns),
            self->ringBuffer_->GetCount(), self->ringBuffer_->GetCapacity(),
            fillMin, fillMax, static_cast<unsigned long long>(writeCalls),
            static_cast<unsigned long long>(writeSamples), maxChunk, tpf,
            nominal, driftPpm);

    // Reset per-interval extremes.
    self->ringFillMin_ = SIZE_MAX;
    self->ringFillMax_ = 0;
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

    // Reset the clock forwarding state so the next streaming session starts
    // fresh; the anchor must be re-captured against the new sample rate.
    clockInitialized_.store(false, std::memory_order_relaxed);
    ourAnchorHostTime_.store(0, std::memory_order_relaxed);
    targetAnchorHostTime_.store(0, std::memory_order_relaxed);
    targetAnchorSampleBits_.store(0, std::memory_order_relaxed);
    targetTsSeq_.store(0, std::memory_order_relaxed);
    targetTsHostTime_.store(0, std::memory_order_relaxed);
    targetTsSampleBits_.store(0, std::memory_order_relaxed);
    zeroTimeStampPeriodCounter_.store(0, std::memory_order_relaxed);
    lastZtsHostTime_.store(0, std::memory_order_relaxed);

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
