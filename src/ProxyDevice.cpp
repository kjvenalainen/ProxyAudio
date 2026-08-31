// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyDevice.hpp"

#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CFBundle.h>
#include <MacTypes.h>
#include <mach/mach_time.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Dispatch.hpp"
#include "Error.hpp"
#include "MuteControl.hpp"
#include "ProxyMuteControl.hpp"
#include "ProxyStream.hpp"
#include "ProxyVolumeControl.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"
#include "VolumeControl.hpp"
#include "aspl/VolumeControl.hpp"

namespace ProxyAudio {

// Number of audio frames the ring buffer can hold.
static constexpr UInt32 kRingBufferFrameCount = 8192;

// This is the period advertised by the proxy and used to model its clock.
static constexpr UInt32 kProxyZeroTimeStampPeriod = 16384;

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
        .ZeroTimeStampPeriod = kProxyZeroTimeStampPeriod,
        .EnableMixing = true,
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
            if (sampleRateChangeInProgress_.load(std::memory_order_acquire)) {
              return;
            }

            // Follow the same path as a client changing the proxy rate. This
            // asks HAL to stop I/O and invokes SetNominalSampleRateImpl only
            // from PerformConfigurationChange.
            const OSStatus status = SetNominalSampleRateAsync(value);
            if (status != noErr) {
              ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
                  ->Message(ProxyAudio::Tracer::Error,
                            "ProxyDevice:sampleRateProxy_() Failed to request "
                            "sample rate %.0f: %d",
                            value, static_cast<int>(status));
            }
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

  // The constructor only initializes the proxy's cached value. Do not use the
  // override here: it is reserved for HAL-approved configuration changes and
  // refreshes streams after the target device has reached the new rate.
  status = aspl::Device::SetNominalSampleRateImpl(sampleRateProxy_.GetValue());
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

      auto mute = std::make_shared<MuteControl>(
          GetContext(), MuteControlParameters{
                            .Scope = kAudioObjectPropertyScopeOutput,
                            .StorageKey = "MUTE_" + GetDeviceUID() + "_" +
                                          std::to_string(streamID),
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

bool ProxyDevice::RefreshProxyStreams() {
  try {
    const auto targetStreamIDs =
        ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
            GetTargetObjectID(),
            {
                .mSelector = kAudioDevicePropertyStreams,
                .mScope = kAudioObjectPropertyScopeOutput,
                .mElement = kAudioObjectPropertyElementMain,
            },
            {});

    if (targetStreamIDs.size() !=
        GetStreamCount(aspl::Direction::Output)) {
      return false;
    }

    for (size_t index = 0; index < targetStreamIDs.size(); ++index) {
      auto stream = std::dynamic_pointer_cast<ProxyStream>(
          GetStreamByIndex(aspl::Direction::Output,
                           static_cast<UInt32>(index)));
      if (!stream || stream->GetTargetObjectID() != targetStreamIDs[index] ||
          stream->RefreshFromTarget() != noErr) {
        return false;
      }
    }

    return true;
  } catch (...) {
    return false;
  }
}

// ---------------------------------------------------------------------------
// ControlRequestHandler: target device IOProc lifecycle
// ---------------------------------------------------------------------------

OSStatus ProxyDevice::OnStartIO() {
  auto tracer = ProxyAudio::Tracer::FromTracer(GetContext()->Tracer);
  tracer->Message(Tracer::Info, "ProxyDevice::OnStartIO() Starting I/O");
  targetIORunning_.store(false, std::memory_order_release);

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

  // This is a temporary clock origin until the target invokes its first I/O
  // callback. TargetIOProc then publishes the real sample-time-zero anchor.
  const UInt64 startHostTime = mach_absolute_time();
  zts_.store({.readTimestamp_ = startHostTime, .zeroTimestampPeriodIndex_ = 0},
             std::memory_order_release);
  totalFramesRead_.store(0, std::memory_order_release);
  underrunCount_ = 0;
  ioProcCallCount_ = 0;
  overrunSampleCount_.store(0, std::memory_order_relaxed);
  writeCallCount_.store(0, std::memory_order_relaxed);
  writeSampleCount_.store(0, std::memory_order_relaxed);
  maxWriteChunkSamples_.store(0, std::memory_order_relaxed);
  ringFillMin_ = SIZE_MAX;
  ringFillMax_ = 0;

  // Create a ring buffer large enough for kRingBufferFrameCount frames.
  // Each frame has outputChannelsPerFrame_ Float32 samples.
  const size_t capacitySamples =
      static_cast<size_t>(kRingBufferFrameCount) * outputChannelsPerFrame_;
  ringBuffer_ = std::make_unique<RingBuffer<Float32>>(capacitySamples);

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStartIO() Ring buffer created: "
                  "capacity=%zu samples, "
                  "channels=%u, sampleRate=%.0f",
                  capacitySamples, outputChannelsPerFrame_, format.mSampleRate);

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

  // Make the ring visible before starting the target: AudioDeviceStart may
  // invoke TargetIOProc before it returns.
  targetIORunning_.store(true, std::memory_order_release);

  // AudioDeviceStart() can block while the target I/O is active. Calling it
  // inline from StartIO holds up HAL's own I/O startup, preventing it from
  // ever issuing WriteMix to this proxy. Start the target on a worker instead
  // and return immediately so HAL can begin its render cycle.
  const auto generation =
      targetIOGeneration_.fetch_add(1, std::memory_order_acq_rel) + 1;
  const auto targetDeviceID = GetTargetObjectID();
  const auto ioProcID = ioProcID_;
  const auto self = std::static_pointer_cast<ProxyDevice>(shared_from_this());
  ProxyAudio::DispatchAsync(^() {
    auto startTracer =
        ProxyAudio::Tracer::FromTracer(self->GetContext()->Tracer);
    startTracer->Message(Tracer::Info,
                         "ProxyDevice::TargetIOStartTask() Starting target "
                         "IOProc on device %u",
                         targetDeviceID);

    const OSStatus startStatus = AudioDeviceStart(targetDeviceID, ioProcID);
    if (startStatus != noErr) {
      startTracer->Message(
          Tracer::Error,
          "ProxyDevice::TargetIOStartTask() AudioDeviceStart returned: %d",
          static_cast<int>(startStatus));
      if (self->targetIOGeneration_.load(std::memory_order_acquire) ==
          generation) {
        self->targetIORunning_.store(false, std::memory_order_release);
      }
      return;
    }

    startTracer->Message(Tracer::Info,
                         "ProxyDevice::TargetIOStartTask() AudioDeviceStart "
                         "returned for device %u",
                         targetDeviceID);
  });

  tracer->Message(Tracer::Info,
                  "ProxyDevice::OnStartIO() Target IOProc start queued on "
                  "device %u",
                  targetDeviceID);
  return kAudioHardwareNoError;
}

void ProxyDevice::OnStopIO() {
  auto tracer = ProxyAudio::Tracer::FromTracer(GetContext()->Tracer);
  tracer->Message(Tracer::Info, "ProxyDevice::OnStopIO() Stopping I/O");

  // Signal both the producer and consumer to stop accessing the ring buffer.
  targetIORunning_.store(false, std::memory_order_release);
  targetIOGeneration_.fetch_add(1, std::memory_order_acq_rel);

  if (ioProcID_ != nullptr) {
    // AudioDeviceStop is synchronous -- after it returns the IOProc is
    // guaranteed to no longer be executing.
    AudioDeviceStop(GetTargetObjectID(), ioProcID_);
    AudioDeviceDestroyIOProcID(GetTargetObjectID(), ioProcID_);
    ioProcID_ = nullptr;
  }

  ringBuffer_.reset();

  tracer->Message(
      Tracer::Info,
      "ProxyDevice::OnStopIO() Final stats: calls=%llu, underruns=%llu, "
      "overrunSamples=%llu",
      static_cast<unsigned long long>(ioProcCallCount_),
      static_cast<unsigned long long>(underrunCount_),
      static_cast<unsigned long long>(
          overrunSampleCount_.load(std::memory_order_relaxed)));

  // Clear device-clock state.
  zts_.store({}, std::memory_order_release);
  totalFramesRead_.store(0, std::memory_order_release);
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
// Device clock: model local TargetIOProc ring-buffer read times
// ---------------------------------------------------------------------------
//
// The startup anchor is local, like NullAudio. TargetIOProc supplies the local
// time that advances this fixed-rate, 16384-frame timeline. The target
// callback's input and output AudioTimeStamps are intentionally ignored.

OSStatus ProxyDevice::GetZeroTimeStampImpl(UInt32 clientID,
                                           Float64* outSampleTime,
                                           UInt64* outHostTime,
                                           UInt64* outSeed) {
  const auto zts = zts_.load(std::memory_order_acquire);

  *outSampleTime = static_cast<Float64>(zts.zeroTimestampPeriodIndex_ *
                                        kProxyZeroTimeStampPeriod);
  *outHostTime = zts.readTimestamp_;
  *outSeed = 1;

  return kAudioHardwareNoError;
}

OSStatus ProxyDevice::WillDoIOOperationImpl(UInt32 clientID,
                                            UInt32 operationID,
                                            Boolean* outWillDo,
                                            Boolean* outWillDoInPlace) {
  switch (operationID) {
    case kAudioServerPlugInIOOperationProcessMix:
    case kAudioServerPlugInIOOperationWriteMix:
      // AddStreamAsync() exposes the stream synchronously but updates
      // libASPL's numOutputStreams_ counter in a later configuration change.
      // The proxy must accept the mix path as soon as its output stream is
      // visible to HAL, otherwise HAL never calls OnWriteMixedOutput().
      *outWillDo = true;
      *outWillDoInPlace = true;
      return kAudioHardwareNoError;

    default:
      return aspl::Device::WillDoIOOperationImpl(clientID, operationID,
                                                 outWillDo, outWillDoInPlace);
  }
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

  // Write as much as possible into the ring buffer. If it overflows, Write
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

  // The target callback supplies the proxy clock's host-time source. Its
  // AudioTimeStamps belong to the target timebase, so use the local host clock.
  const UInt64 readHostTime = mach_absolute_time();

  // Track ring fill extremes before we read anything, for diagnostics.
  {
    const size_t fillBefore = self->ringBuffer_->GetCount();
    if (fillBefore < self->ringFillMin_)
      self->ringFillMin_ = fillBefore;
    if (fillBefore > self->ringFillMax_)
      self->ringFillMax_ = fillBefore;
  }

  // Fill every output buffer from the ring buffer. If an underrun occurs,
  // pad the remainder with silence.
  UInt32 underrunSamplesThisCall = 0;
  UInt64 framesReadThisCall = 0;
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

    framesReadThisCall += samplesNeeded / self->outputChannelsPerFrame_;
  }

  // Count the frames the target just consumed. The first target callback is
  // the real sample-time-zero anchor. Without this update, the delay between
  // OnStartIO() and the first target callback is incorrectly included in the
  // first period, making the published clock appear slower than its nominal
  // sample rate.
  const auto oldTotalFramesRead =
      self->totalFramesRead_.fetch_add(framesReadThisCall);
  const auto newTotalFramesRead = oldTotalFramesRead + framesReadThisCall;
  if (oldTotalFramesRead == 0 && framesReadThisCall != 0) {
    self->zts_.store(
        {.readTimestamp_ = readHostTime, .zeroTimestampPeriodIndex_ = 0},
        std::memory_order_release);
  }

  // Publish one zero timestamp for each completed period. The stored value is
  // a period index, not a frame count: GetZeroTimeStampImpl() multiplies it by
  // kProxyZeroTimeStampPeriod to form outSampleTime.
  const auto oldPeriodIndex = oldTotalFramesRead / kProxyZeroTimeStampPeriod;
  const auto newPeriodIndex = newTotalFramesRead / kProxyZeroTimeStampPeriod;
  if (newPeriodIndex != oldPeriodIndex) {
    self->zts_.store({.readTimestamp_ = readHostTime,
                      .zeroTimestampPeriodIndex_ = newPeriodIndex},
                     std::memory_order_release);
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

  // Periodically log overall streaming health.
  if (self->ioProcCallCount_ % 500 == 0) {
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
            "writeCalls=%llu writeSamples=%llu maxWriteChunk=%u",
            static_cast<unsigned long long>(self->ioProcCallCount_),
            static_cast<unsigned long long>(self->underrunCount_),
            static_cast<unsigned long long>(overruns),
            self->ringBuffer_->GetCount(), self->ringBuffer_->GetCapacity(),
            fillMin, fillMax, static_cast<unsigned long long>(writeCalls),
            static_cast<unsigned long long>(writeSamples), maxChunk);

    // Reset per-interval extremes.
    self->ringFillMin_ = SIZE_MAX;
    self->ringFillMax_ = 0;
  }

  return noErr;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

OSStatus ProxyDevice::SetNominalSampleRateImpl(Float64 rate) {
  // SetNominalSampleRateAsync arranges for this to run from HAL's
  // PerformConfigurationChange callback, after outstanding I/O has stopped.
  return ApplySampleRateChange(rate);
}

OSStatus ProxyDevice::ApplySampleRateChange(Float64 value) {
  // A target change notification can be delivered synchronously while we set
  // its property. It describes the same transition, so only the outer change
  // applies the new device and stream state.
  if (sampleRateChangeInProgress_.exchange(true, std::memory_order_acq_rel)) {
    return aspl::Device::SetNominalSampleRateImpl(value);
  }

  const auto clearInProgress = [this]() {
    sampleRateChangeInProgress_.store(false, std::memory_order_release);
  };

  try {
    // A client may have changed the proxy rate, in which case the target must
    // be changed first. For a target property notification it is already at
    // the requested rate and this is a no-op.
    if (sampleRateProxy_.GetValue() != value) {
      sampleRateProxy_.SetValue(value);
      if (sampleRateProxy_.GetValue() != value) {
        clearInProgress();
        return kAudioDeviceUnsupportedFormatError;
      }
    }

    // Samples and timestamps from the old rate must not cross into the new
    // target format. The HAL performs the corresponding stop/start cycle.
    targetIORunning_.store(false, std::memory_order_release);
    zts_.store({}, std::memory_order_release);

    const OSStatus status = aspl::Device::SetNominalSampleRateImpl(value);

    if (status == noErr && !RefreshProxyStreams()) {
      // A nominal-rate change normally leaves stream identity and topology
      // intact, as in NullAudio. Rebuild only when the target actually changed
      // its stream list or immutable stream parameters.
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ApplySampleRateChange() Target stream "
                    "topology changed; rebuilding proxy streams");
      RemoveStreams();
      AddProxyStreams();
    }

    // Do not call PropertiesChanged here. HAL compares the device and stream
    // properties after PerformConfigurationChange returns, just as NullAudio
    // relies on it to publish nominal-rate and stream-format changes.

    clearInProgress();
    return status;
  } catch (const OSStatusError& e) {
    clearInProgress();
    return e.GetStatus();
  } catch (...) {
    clearInProgress();
    return kAudioHardwareUnspecifiedError;
  }
}

}  // namespace ProxyAudio
