// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioServerPlugIn.h>

#include <aspl/Context.hpp>
#include <aspl/Plugin.hpp>
#include <aspl/Tracer.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "Dispatch.hpp"
#include "ProxyDevice.hpp"
#include "Tracer.hpp"

namespace ProxyAudio::Test {

AudioObjectPropertyAddress Address(AudioObjectPropertySelector selector,
                                   AudioObjectPropertyScope scope =
                                       kAudioObjectPropertyScopeGlobal,
                                   AudioObjectPropertyElement element =
                                       kAudioObjectPropertyElementMain);

AudioStreamBasicDescription Float32Format(Float64 sampleRate = 48000,
                                          UInt32 channels = 2);

class FakeAudioHardware {
 public:
  enum class Operation {
    GetSize,
    Get,
    Set,
    AddListener,
    RemoveListener,
    CreateIOProc,
    StartIOProc,
    StopIOProc,
    DestroyIOProc,
  };

  struct Call {
    Operation operation;
    AudioObjectID objectID;
    AudioObjectPropertyAddress address;
    OSStatus status;
  };

  struct IOResult {
    OSStatus status = noErr;
    std::vector<std::vector<Float32>> buffers;
  };

  FakeAudioHardware();
  ~FakeAudioHardware();

  FakeAudioHardware(const FakeAudioHardware&) = delete;
  FakeAudioHardware& operator=(const FakeAudioHardware&) = delete;

  template <typename T>
  void Set(AudioObjectID objectID,
           const AudioObjectPropertyAddress& address,
           const T& value) {
    SetBytes(objectID, address, &value, sizeof(value));
  }

  template <typename T>
  void SetVector(AudioObjectID objectID,
                 const AudioObjectPropertyAddress& address,
                 const std::vector<T>& values) {
    SetBytes(objectID, address, values.data(),
             static_cast<UInt32>(values.size() * sizeof(T)));
  }

  template <typename T>
  T Value(AudioObjectID objectID,
          const AudioObjectPropertyAddress& address) const {
    const auto bytes = Bytes(objectID, address);
    if (bytes.size() != sizeof(T)) {
      throw std::runtime_error("fake property has unexpected size");
    }
    T value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
  }

  void SetString(AudioObjectID objectID,
                 const AudioObjectPropertyAddress& address,
                 std::string value);
  void Erase(AudioObjectID objectID,
             const AudioObjectPropertyAddress& address);
  void Notify(AudioObjectID objectID,
              const AudioObjectPropertyAddress& address);
  void NotifyWhenSet(AudioObjectID objectID,
                     const AudioObjectPropertyAddress& address,
                     bool enabled = true);

  void FailNext(Operation operation,
                OSStatus status,
                AudioObjectID objectID = kAudioObjectUnknown,
                std::optional<AudioObjectPropertyAddress> address = {});
  void QueueStartResult(OSStatus status);

  size_t ListenerCount() const;
  size_t ListenerCount(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address) const;
  size_t CallCount(Operation operation) const;
  std::vector<Call> Calls() const;

  IOResult InvokeCurrentIOProc(AudioObjectID deviceID,
                               const std::vector<size_t>& bufferSamples,
                               Float32 initialValue = -1.0f);
  IOResult InvokeLastDestroyedIOProc(
      AudioObjectID deviceID,
      const std::vector<size_t>& bufferSamples,
      Float32 initialValue = -1.0f);

  OSStatus GetPropertyDataSize(AudioObjectID objectID,
                               const AudioObjectPropertyAddress& address,
                               UInt32* outSize);
  OSStatus GetPropertyData(AudioObjectID objectID,
                           const AudioObjectPropertyAddress& address,
                           UInt32* ioSize,
                           void* outData);
  OSStatus SetPropertyData(AudioObjectID objectID,
                           const AudioObjectPropertyAddress& address,
                           UInt32 size,
                           const void* data);
  OSStatus AddListener(AudioObjectID objectID,
                       const AudioObjectPropertyAddress& address,
                       AudioObjectPropertyListenerProc listener,
                       void* clientData);
  OSStatus RemoveListener(AudioObjectID objectID,
                          const AudioObjectPropertyAddress& address,
                          AudioObjectPropertyListenerProc listener,
                          void* clientData);
  OSStatus CreateIOProc(AudioObjectID deviceID,
                        AudioDeviceIOProc proc,
                        void* clientData,
                        AudioDeviceIOProcID* outID);
  OSStatus StartIOProc(AudioObjectID deviceID, AudioDeviceIOProcID id);
  OSStatus StopIOProc(AudioObjectID deviceID, AudioDeviceIOProcID id);
  OSStatus DestroyIOProc(AudioObjectID deviceID, AudioDeviceIOProcID id);

  static FakeAudioHardware& Active();

 private:
  struct PropertyKey {
    AudioObjectID objectID;
    AudioObjectPropertyAddress address;

    bool operator==(const PropertyKey& other) const;
  };

  struct PropertyKeyHash {
    size_t operator()(const PropertyKey& key) const;
  };

  struct PropertyValue {
    std::vector<std::byte> bytes;
    std::optional<std::string> string;
  };

  struct Listener {
    PropertyKey key;
    AudioObjectPropertyListenerProc callback;
    void* clientData;
  };

  struct Failure {
    Operation operation;
    AudioObjectID objectID;
    std::optional<AudioObjectPropertyAddress> address;
    OSStatus status;
  };

  struct IOProc {
    AudioObjectID deviceID;
    AudioDeviceIOProc proc;
    void* clientData;
    AudioDeviceIOProcID id;
    bool started = false;
    bool destroyed = false;
  };

  void SetBytes(AudioObjectID objectID,
                const AudioObjectPropertyAddress& address,
                const void* bytes,
                UInt32 size);
  std::vector<std::byte> Bytes(
      AudioObjectID objectID,
      const AudioObjectPropertyAddress& address) const;
  OSStatus ConsumeFailure(Operation operation,
                          AudioObjectID objectID,
                          const std::optional<AudioObjectPropertyAddress>&
                              address = {});
  void Record(Operation operation,
              AudioObjectID objectID,
              const AudioObjectPropertyAddress& address,
              OSStatus status);
  IOResult Invoke(const IOProc& ioProc,
                  const std::vector<size_t>& bufferSamples,
                  Float32 initialValue);

  static FakeAudioHardware* active_;

  mutable std::mutex mutex_;
  std::unordered_map<PropertyKey, PropertyValue, PropertyKeyHash> properties_;
  std::vector<Listener> listeners_;
  std::vector<Failure> failures_;
  std::vector<Call> calls_;
  std::vector<PropertyKey> notifyOnSet_;
  std::vector<IOProc> ioProcs_;
  std::optional<IOProc> lastDestroyedIOProc_;
  std::deque<OSStatus> startResults_;
};

class ManualDispatch {
 public:
  ManualDispatch();
  ~ManualDispatch();

  ManualDispatch(const ManualDispatch&) = delete;
  ManualDispatch& operator=(const ManualDispatch&) = delete;

  void DrainOne();
  void DrainAll();
  void DrainDelayed();
  size_t Pending() const;
  size_t PendingDelayed() const;

  void Enqueue(dispatch_block_t block, bool delayed);
  static ManualDispatch& Active();

 private:
  static ManualDispatch* active_;
  mutable std::mutex mutex_;
  std::deque<dispatch_block_t> pending_;
  std::deque<dispatch_block_t> delayed_;
};

void SetFakeHostTime(UInt64 value);
void AdvanceFakeHostTime(UInt64 amount);

class FakePluginHost {
 public:
  struct Notification {
    AudioObjectID objectID;
    std::vector<AudioObjectPropertyAddress> addresses;
  };

  FakePluginHost();
  ~FakePluginHost();

  AudioServerPlugInHostRef Ref();
  size_t NotificationCount() const;
  size_t NotificationCount(AudioObjectID objectID) const;
  const std::vector<Notification>& Notifications() const;
  const std::vector<std::pair<AudioObjectID, UInt64>>&
  ConfigurationRequests() const;
  void CompleteConfigurationRequests(
      const std::shared_ptr<aspl::Plugin>& plugin);

  static FakePluginHost& Active();

 private:
  static OSStatus PropertiesChanged(
      AudioServerPlugInHostRef host,
      AudioObjectID objectID,
      UInt32 addressCount,
      const AudioObjectPropertyAddress* addresses);
  static OSStatus CopyFromStorage(AudioServerPlugInHostRef host,
                                  CFStringRef key,
                                  CFPropertyListRef* data);
  static OSStatus WriteToStorage(AudioServerPlugInHostRef host,
                                 CFStringRef key,
                                 CFPropertyListRef data);
  static OSStatus DeleteFromStorage(AudioServerPlugInHostRef host,
                                    CFStringRef key);
  static OSStatus RequestDeviceConfigurationChange(
      AudioServerPlugInHostRef host,
      AudioObjectID deviceID,
      UInt64 action,
      void* info);

  static FakePluginHost* active_;
  AudioServerPlugInHostInterface interface_{};
  std::vector<Notification> notifications_;
  std::vector<std::pair<AudioObjectID, UInt64>> configurationRequests_;
  size_t completedConfigurationRequests_ = 0;
};

class ProxyDeviceTestAccess {
 public:
  static bool IsRunning(const ProxyDevice& device);
  static UInt64 Generation(const ProxyDevice& device);
  static UInt64 FramesRead(const ProxyDevice& device);
  static UInt64 Underruns(const ProxyDevice& device);
  static UInt64 IOProcCalls(const ProxyDevice& device);
  static UInt64 OverrunSamples(const ProxyDevice& device);
  static UInt64 WriteCalls(const ProxyDevice& device);
  static UInt64 WriteSamples(const ProxyDevice& device);
  static UInt32 MaxWriteChunk(const ProxyDevice& device);
  static size_t RingCount(const ProxyDevice& device);
  static size_t RingCapacity(const ProxyDevice& device);
  static size_t RingFillMinimum(const ProxyDevice& device);
  static size_t RingFillMaximum(const ProxyDevice& device);
  static ZeroTimestamp Timestamp(const ProxyDevice& device);
  static OSStatus ApplyRate(ProxyDevice& device, Float64 rate);
};

struct DeviceSpec {
  std::string name = "Output";
  std::string uid;
  Float64 sampleRate = 48000;
  UInt32 channels = 2;
  UInt32 latency = 70;
  UInt32 streamLatency = 11;
  bool hasOutputStream = true;
  bool hasVolume = true;
  bool hasMute = true;
  Float32 volume = 0.121945f;
  Float64 minimumDecibels = -63.5;
  UInt32 muted = 0;
  AudioClassID classID = kAudioDeviceClassID;
};

// A concise hand-written scenario API. Future log-derived regressions can add
// aliases and operations without introducing a log parser or replay format.
class ProxyAudioScenario {
 public:
  ProxyAudioScenario();
  ~ProxyAudioScenario();

  AudioObjectID AddDevice(const std::string& alias,
                          DeviceSpec spec = {});
  AudioObjectID DeviceID(const std::string& alias) const;
  AudioObjectID StreamID(const std::string& alias) const;
  AudioObjectID VolumeID(const std::string& alias) const;
  AudioObjectID MuteID(const std::string& alias) const;
  void SetSystemDevices(const std::vector<std::string>& aliases);
  std::shared_ptr<ProxyDevice> CreateProxy(const std::string& alias);
  std::shared_ptr<ProxyDevice> Proxy(const std::string& alias) const;
  void SetTargetVolume(const std::string& alias,
                       Float32 value,
                       bool notify = true);
  Float32 TargetVolume(const std::string& alias) const;
  void Write(const std::string& alias, const std::vector<Float32>& samples);
  FakeAudioHardware::IOResult ReadTarget(
      const std::string& alias,
      const std::vector<size_t>& bufferSamples,
      Float32 initialValue = -1.0f);
  void Start(const std::string& alias);
  void Stop(const std::string& alias);

  FakeAudioHardware hardware;
  ManualDispatch dispatch;
  std::shared_ptr<aspl::Context> context;

 private:
  struct IDs {
    AudioObjectID device = kAudioObjectUnknown;
    AudioObjectID stream = kAudioObjectUnknown;
    AudioObjectID volume = kAudioObjectUnknown;
    AudioObjectID mute = kAudioObjectUnknown;
  };

  AudioObjectID nextID_ = 100;
  std::unordered_map<std::string, IDs> ids_;
  std::unordered_map<std::string, std::shared_ptr<ProxyDevice>> proxies_;
};

}  // namespace ProxyAudio::Test
