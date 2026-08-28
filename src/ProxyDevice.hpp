// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <MacTypes.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "ProxyObject.hpp"
#include "ProxyProperty.hpp"
#include "RingBuffer.hpp"

namespace ProxyAudio {

struct ZeroTimestamp {
  // Host-based timestamp of the last time the read(s) looped over the virtual
  // ring buffer.
  UInt64 readTimestamp_ = 0;
  // Index of the number of loops that have occurred.
  UInt64 zeroTimestampPeriodIndex_ = 0;
};

struct ProxyDevice;

// Explicit specialization MUST come before ProxyDevice inherits from
// ProxyObject<ProxyDevice> Otherwise BaseTraits<ProxyDevice> gets instantiated
// with the default template.
template <>
struct BaseTraits<ProxyDevice> {
  typedef aspl::Device BaseType;
  typedef aspl::DeviceParameters ParametersType;
};

static constexpr AudioObjectPropertyAddress DeviceLatencyAddress = {
    .mSelector = kAudioDevicePropertyLatency,
    .mScope = kAudioObjectPropertyScopeOutput,
    .mElement = kAudioObjectPropertyElementMain};

static constexpr AudioObjectPropertyAddress DeviceSampleRateAddress = {
    .mSelector = kAudioDevicePropertyNominalSampleRate,
    .mScope = kAudioObjectPropertyScopeGlobal,
    .mElement = kAudioObjectPropertyElementMain};

static constexpr AudioObjectPropertyAddress DeviceAvailableSampleRatesAddress =
    {.mSelector = kAudioDevicePropertyAvailableNominalSampleRates,
     .mScope = kAudioObjectPropertyScopeGlobal,
     .mElement = kAudioObjectPropertyElementMain};

// A aspl::Device which clones all of the Audio Object properties from the
// target device on creation.
class ProxyDevice : public ProxyObject<ProxyDevice>,
                    public aspl::IORequestHandler,
                    public aspl::ControlRequestHandler {
  friend struct ProxyObject<ProxyDevice>;

  struct private_tag {};

 protected:
  static aspl::DeviceParameters GetParameters(
      const AudioObjectID targetDeviceID,
      std::shared_ptr<const aspl::Context> context);

 public:
  static std::shared_ptr<ProxyDevice> Create(
      const AudioObjectID targetObjectID,
      std::shared_ptr<const aspl::Context> context);

  explicit ProxyDevice(private_tag,
                       const AudioObjectID targetObjectID,
                       std::shared_ptr<const aspl::Context> context);

  virtual ~ProxyDevice();

  // Get the version of the proxy device (IE driver version).
  CFStringRef GetVersion() const;

  // ControlRequestHandler overrides -- manage target device IOProc lifecycle.
  OSStatus OnStartIO() override;
  void OnStopIO() override;

  // IORequestHandler override -- buffer incoming mixed audio for target device.
  void OnWriteMixedOutput(const std::shared_ptr<aspl::Stream>& stream,
                          Float64 zeroTimestamp,
                          Float64 timestamp,
                          const void* bytes,
                          UInt32 bytesCount) override;

  // Override the device clock using the target IOProc's local read times.
  OSStatus GetZeroTimeStampImpl(UInt32 clientID,
                                Float64* outSampleTime,
                                UInt64* outHostTime,
                                UInt64* outSeed) override;

 protected:
  // Adds all streams from the target device to the proxy device
  // as proxy streams.
  void AddProxyStreams();

  // Remove all streams from the proxy device.
  void RemoveStreams();

  // Perform a sample rate change, recreating all streams with the new sample
  // rate after the target device changes. This is called via
  // RequestConfigurationChange.
  void PerformSampleRateChange(const Float64& value);

  // Applies a rate change in either direction. A system request needs to set
  // the target device first; a target property notification is already at the
  // requested rate and only needs to refresh the proxy.
  OSStatus ApplySampleRateChange(Float64 value, bool setTargetSampleRate);

  // Propagate system-initiated proxy rate changes to the target before
  // refreshing streams from its new format.
  OSStatus SetNominalSampleRateImpl(Float64 rate) override;

  ProxyProperty<Float64> sampleRateProxy_;

 private:
  // CoreAudio IOProc callback registered on the target hardware device.
  // Reads buffered audio from the ring buffer and writes it to the target
  // device's output buffers. Fills with silence on underrun.
  static OSStatus TargetIOProc(AudioObjectID inDevice,
                               const AudioTimeStamp* inNow,
                               const AudioBufferList* inInputData,
                               const AudioTimeStamp* inInputTime,
                               AudioBufferList* outOutputData,
                               const AudioTimeStamp* inOutputTime,
                               void* inClientData);

  // Ring buffer that decouples the proxy device's I/O cycle from the target
  // device's I/O cycle. Stores Float32 samples so that volume / mute
  // processing can be applied in the float domain before playback.
  // Written by OnWriteMixedOutput (producer), read by TargetIOProc (consumer).
  std::unique_ptr<RingBuffer<Float32>> ringBuffer_;

  // IOProc ID registered on the target device. Non-null while streaming.
  AudioDeviceIOProcID ioProcID_ = nullptr;

  // Whether the target device IOProc is actively streaming. Checked by both
  // the producer and consumer to gate ring buffer access.
  std::atomic<bool> targetIORunning_{false};

  // Number of interleaved channels per frame for the current output format.
  UInt32 outputChannelsPerFrame_ = 0;

  // --- Device clock state -------------------------------------------------

  // The zero-timestamp of the device.
  std::atomic<ZeroTimestamp> zts_;

  // Cumulative frames read since the start of streaming.
  std::atomic<UInt64> totalFramesRead_;

  // Prevent the target's rate-change notification from recursively applying
  // the same reconfiguration while this proxy is setting that target rate.
  std::atomic<bool> sampleRateChangeInProgress_{false};

  // Diagnostic counters, updated on the IOProc thread and periodically
  // published to the tracer. Not used for correctness.
  UInt64 underrunCount_ = 0;
  UInt64 ioProcCallCount_ = 0;

  // Overrun counter on the producer (OnWriteMixedOutput) side.
  std::atomic<UInt64> overrunSampleCount_{0};
  std::atomic<UInt64> writeCallCount_{0};
  std::atomic<UInt64> writeSampleCount_{0};
  std::atomic<UInt32> maxWriteChunkSamples_{0};

  // Ring-fill tracking across the current reporting interval.
  size_t ringFillMin_ = SIZE_MAX;
  size_t ringFillMax_ = 0;
};

}  // namespace ProxyAudio
