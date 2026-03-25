// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <MacTypes.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <atomic>
#include <memory>

#include "ProxyObject.hpp"
#include "ProxyProperty.hpp"
#include "RingBuffer.hpp"

namespace ProxyAudio {

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

  // Override the device clock to steer write-rate based on ring buffer level.
  // When the buffer fills above target we advance the reported host time,
  // making the HAL perceive a slower device clock and reduce its write rate.
  // When the buffer drains below target we do the opposite.
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
  // rate. This should be called via RequestConfigurationChange.
  void PerformSampleRateChange(const Float64& value);

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

  // --- Adaptive clock state (used by GetZeroTimeStampImpl) ----------------

  // Mach host ticks corresponding to one audio frame at the current sample
  // rate.  Computed once in OnStartIO.
  Float64 hostTicksPerFrame_ = 0.0;

  // Accumulated clock offset in host ticks.  Positive values advance the
  // reported host time (HAL perceives a slower device → reduces write rate),
  // negative values retard it (HAL perceives a faster device → increases
  // write rate).
  Float64 clockOffset_ = 0.0;
};

}  // namespace ProxyAudio
