// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "TestSupport.hpp"

#include <Block.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "HostTime.hpp"

namespace ProxyAudio::Test {
namespace {

bool EqualAddress(const AudioObjectPropertyAddress& left,
                  const AudioObjectPropertyAddress& right) {
  return left.mSelector == right.mSelector && left.mScope == right.mScope &&
         left.mElement == right.mElement;
}

const AudioObjectPropertyAddress kNoAddress{};

std::atomic<UInt64> gHostTime{0};

}  // namespace

AudioObjectPropertyAddress Address(AudioObjectPropertySelector selector,
                                   AudioObjectPropertyScope scope,
                                   AudioObjectPropertyElement element) {
  return {.mSelector = selector, .mScope = scope, .mElement = element};
}

AudioStreamBasicDescription Float32Format(Float64 sampleRate,
                                          UInt32 channels) {
  AudioStreamBasicDescription format{};
  format.mSampleRate = sampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  format.mBytesPerPacket = channels * sizeof(Float32);
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = channels * sizeof(Float32);
  format.mChannelsPerFrame = channels;
  format.mBitsPerChannel = 32;
  return format;
}

FakeAudioHardware* FakeAudioHardware::active_ = nullptr;

FakeAudioHardware::FakeAudioHardware() {
  if (active_) {
    throw std::runtime_error("only one FakeAudioHardware may be active");
  }
  active_ = this;
}

FakeAudioHardware::~FakeAudioHardware() {
  active_ = nullptr;
}

FakeAudioHardware& FakeAudioHardware::Active() {
  if (!active_) {
    throw std::runtime_error("no FakeAudioHardware is active");
  }
  return *active_;
}

bool FakeAudioHardware::PropertyKey::operator==(
    const PropertyKey& other) const {
  return objectID == other.objectID && EqualAddress(address, other.address);
}

size_t FakeAudioHardware::PropertyKeyHash::operator()(
    const PropertyKey& key) const {
  size_t value = std::hash<AudioObjectID>{}(key.objectID);
  value ^= std::hash<UInt32>{}(key.address.mSelector) + 0x9e3779b9 +
           (value << 6) + (value >> 2);
  value ^= std::hash<UInt32>{}(key.address.mScope) + 0x9e3779b9 +
           (value << 6) + (value >> 2);
  value ^= std::hash<UInt32>{}(key.address.mElement) + 0x9e3779b9 +
           (value << 6) + (value >> 2);
  return value;
}

void FakeAudioHardware::SetBytes(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    const void* bytes,
    UInt32 size) {
  PropertyValue property;
  property.bytes.resize(size);
  if (size != 0) {
    std::memcpy(property.bytes.data(), bytes, size);
  }

  std::lock_guard lock(mutex_);
  properties_[{objectID, address}] = std::move(property);
}

std::vector<std::byte> FakeAudioHardware::Bytes(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address) const {
  std::lock_guard lock(mutex_);
  const auto found = properties_.find({objectID, address});
  if (found == properties_.end() || found->second.string) {
    throw std::runtime_error("fake property is missing or is a string");
  }
  return found->second.bytes;
}

void FakeAudioHardware::SetString(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    std::string value) {
  PropertyValue property;
  property.bytes.resize(sizeof(CFStringRef));
  property.string = std::move(value);

  std::lock_guard lock(mutex_);
  properties_[{objectID, address}] = std::move(property);
}

void FakeAudioHardware::Erase(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address) {
  std::lock_guard lock(mutex_);
  properties_.erase({objectID, address});
}

void FakeAudioHardware::Notify(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address) {
  std::vector<Listener> callbacks;
  {
    std::lock_guard lock(mutex_);
    for (const auto& listener : listeners_) {
      if (listener.key == PropertyKey{objectID, address}) {
        callbacks.push_back(listener);
      }
    }
  }

  for (const auto& listener : callbacks) {
    listener.callback(objectID, 1, &address, listener.clientData);
  }
}

void FakeAudioHardware::NotifyWhenSet(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    bool enabled) {
  const PropertyKey key{objectID, address};
  std::lock_guard lock(mutex_);
  const auto found = std::find(notifyOnSet_.begin(), notifyOnSet_.end(), key);
  if (enabled && found == notifyOnSet_.end()) {
    notifyOnSet_.push_back(key);
  } else if (!enabled && found != notifyOnSet_.end()) {
    notifyOnSet_.erase(found);
  }
}

void FakeAudioHardware::FailNext(
    Operation operation,
    OSStatus status,
    AudioObjectID objectID,
    std::optional<AudioObjectPropertyAddress> address) {
  std::lock_guard lock(mutex_);
  failures_.push_back({operation, objectID, address, status});
}

void FakeAudioHardware::QueueStartResult(OSStatus status) {
  std::lock_guard lock(mutex_);
  startResults_.push_back(status);
}

OSStatus FakeAudioHardware::ConsumeFailure(
    Operation operation,
    AudioObjectID objectID,
    const std::optional<AudioObjectPropertyAddress>& address) {
  const auto found = std::find_if(
      failures_.begin(), failures_.end(), [&](const Failure& failure) {
        const bool objectMatches =
            failure.objectID == kAudioObjectUnknown ||
            failure.objectID == objectID;
        const bool addressMatches =
            !failure.address ||
            (address && EqualAddress(*failure.address, *address));
        return failure.operation == operation && objectMatches &&
               addressMatches;
      });
  if (found == failures_.end()) {
    return noErr;
  }
  const auto status = found->status;
  failures_.erase(found);
  return status;
}

void FakeAudioHardware::Record(
    Operation operation,
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    OSStatus status) {
  calls_.push_back({operation, objectID, address, status});
}

size_t FakeAudioHardware::ListenerCount() const {
  std::lock_guard lock(mutex_);
  return listeners_.size();
}

size_t FakeAudioHardware::ListenerCount(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address) const {
  std::lock_guard lock(mutex_);
  return std::count_if(listeners_.begin(), listeners_.end(),
                       [&](const Listener& listener) {
                         return listener.key == PropertyKey{objectID, address};
                       });
}

size_t FakeAudioHardware::CallCount(Operation operation) const {
  std::lock_guard lock(mutex_);
  return std::count_if(calls_.begin(), calls_.end(), [&](const Call& call) {
    return call.operation == operation;
  });
}

std::vector<FakeAudioHardware::Call> FakeAudioHardware::Calls() const {
  std::lock_guard lock(mutex_);
  return calls_;
}

OSStatus FakeAudioHardware::GetPropertyDataSize(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    UInt32* outSize) {
  std::lock_guard lock(mutex_);
  auto status = ConsumeFailure(Operation::GetSize, objectID, address);
  const auto found = properties_.find({objectID, address});
  if (status == noErr && found == properties_.end()) {
    status = kAudioHardwareUnknownPropertyError;
  }
  if (status == noErr) {
    *outSize = static_cast<UInt32>(found->second.bytes.size());
  }
  Record(Operation::GetSize, objectID, address, status);
  return status;
}

OSStatus FakeAudioHardware::GetPropertyData(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    UInt32* ioSize,
    void* outData) {
  std::lock_guard lock(mutex_);
  auto status = ConsumeFailure(Operation::Get, objectID, address);
  const auto found = properties_.find({objectID, address});
  if (status == noErr && found == properties_.end()) {
    status = kAudioHardwareUnknownPropertyError;
  }
  if (status == noErr && *ioSize < found->second.bytes.size()) {
    status = kAudioHardwareBadPropertySizeError;
  }
  if (status == noErr) {
    if (found->second.string) {
      auto value = CFStringCreateWithCString(
          kCFAllocatorDefault, found->second.string->c_str(),
          kCFStringEncodingUTF8);
      std::memcpy(outData, &value, sizeof(value));
      *ioSize = sizeof(value);
    } else {
      std::memcpy(outData, found->second.bytes.data(),
                  found->second.bytes.size());
      *ioSize = static_cast<UInt32>(found->second.bytes.size());
    }
  }
  Record(Operation::Get, objectID, address, status);
  return status;
}

OSStatus FakeAudioHardware::SetPropertyData(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    UInt32 size,
    const void* data) {
  bool shouldNotify = false;
  OSStatus status = noErr;
  {
    std::lock_guard lock(mutex_);
    status = ConsumeFailure(Operation::Set, objectID, address);
    if (status == noErr) {
      PropertyValue value;
      value.bytes.resize(size);
      if (size != 0) {
        std::memcpy(value.bytes.data(), data, size);
      }
      properties_[{objectID, address}] = std::move(value);
      shouldNotify =
          std::find(notifyOnSet_.begin(), notifyOnSet_.end(),
                    PropertyKey{objectID, address}) != notifyOnSet_.end();
    }
    Record(Operation::Set, objectID, address, status);
  }
  if (status == noErr && shouldNotify) {
    Notify(objectID, address);
  }
  return status;
}

OSStatus FakeAudioHardware::AddListener(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    AudioObjectPropertyListenerProc listener,
    void* clientData) {
  std::lock_guard lock(mutex_);
  const auto status =
      ConsumeFailure(Operation::AddListener, objectID, address);
  if (status == noErr) {
    listeners_.push_back({{objectID, address}, listener, clientData});
  }
  Record(Operation::AddListener, objectID, address, status);
  return status;
}

OSStatus FakeAudioHardware::RemoveListener(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress& address,
    AudioObjectPropertyListenerProc listener,
    void* clientData) {
  std::lock_guard lock(mutex_);
  const auto status =
      ConsumeFailure(Operation::RemoveListener, objectID, address);
  if (status == noErr) {
    const auto found = std::find_if(
        listeners_.begin(), listeners_.end(), [&](const Listener& item) {
          return item.key == PropertyKey{objectID, address} &&
                 item.callback == listener && item.clientData == clientData;
        });
    if (found != listeners_.end()) {
      listeners_.erase(found);
    }
  }
  Record(Operation::RemoveListener, objectID, address, status);
  return status;
}

OSStatus FakeAudioHardware::CreateIOProc(
    AudioObjectID deviceID,
    AudioDeviceIOProc proc,
    void* clientData,
    AudioDeviceIOProcID* outID) {
  std::lock_guard lock(mutex_);
  const auto status = ConsumeFailure(Operation::CreateIOProc, deviceID);
  if (status == noErr) {
    *outID = proc;
    ioProcs_.push_back({deviceID, proc, clientData, *outID});
  }
  Record(Operation::CreateIOProc, deviceID, kNoAddress, status);
  return status;
}

OSStatus FakeAudioHardware::StartIOProc(AudioObjectID deviceID,
                                        AudioDeviceIOProcID id) {
  std::lock_guard lock(mutex_);
  OSStatus status = noErr;
  if (!startResults_.empty()) {
    status = startResults_.front();
    startResults_.pop_front();
  } else {
    status = ConsumeFailure(Operation::StartIOProc, deviceID);
  }
  if (status == noErr) {
    const auto found = std::find_if(ioProcs_.rbegin(), ioProcs_.rend(),
                                    [&](const IOProc& item) {
                                      return item.deviceID == deviceID &&
                                             item.id == id && !item.destroyed;
                                    });
    if (found != ioProcs_.rend()) {
      found->started = true;
    }
  }
  Record(Operation::StartIOProc, deviceID, kNoAddress, status);
  return status;
}

OSStatus FakeAudioHardware::StopIOProc(AudioObjectID deviceID,
                                       AudioDeviceIOProcID id) {
  std::lock_guard lock(mutex_);
  const auto status = ConsumeFailure(Operation::StopIOProc, deviceID);
  if (status == noErr) {
    const auto found = std::find_if(ioProcs_.rbegin(), ioProcs_.rend(),
                                    [&](const IOProc& item) {
                                      return item.deviceID == deviceID &&
                                             item.id == id && !item.destroyed;
                                    });
    if (found != ioProcs_.rend()) {
      found->started = false;
    }
  }
  Record(Operation::StopIOProc, deviceID, kNoAddress, status);
  return status;
}

OSStatus FakeAudioHardware::DestroyIOProc(AudioObjectID deviceID,
                                          AudioDeviceIOProcID id) {
  std::lock_guard lock(mutex_);
  const auto status = ConsumeFailure(Operation::DestroyIOProc, deviceID);
  if (status == noErr) {
    const auto found = std::find_if(ioProcs_.rbegin(), ioProcs_.rend(),
                                    [&](const IOProc& item) {
                                      return item.deviceID == deviceID &&
                                             item.id == id && !item.destroyed;
                                    });
    if (found != ioProcs_.rend()) {
      found->destroyed = true;
      lastDestroyedIOProc_ = *found;
    }
  }
  Record(Operation::DestroyIOProc, deviceID, kNoAddress, status);
  return status;
}

FakeAudioHardware::IOResult FakeAudioHardware::Invoke(
    const IOProc& ioProc,
    const std::vector<size_t>& bufferSamples,
    Float32 initialValue) {
  IOResult result;
  result.buffers.reserve(bufferSamples.size());
  for (const auto count : bufferSamples) {
    result.buffers.emplace_back(count, initialValue);
  }

  const auto listBytes = offsetof(AudioBufferList, mBuffers) +
                         sizeof(AudioBuffer) * bufferSamples.size();
  auto* list = static_cast<AudioBufferList*>(std::calloc(1, listBytes));
  if (!list) {
    throw std::bad_alloc();
  }
  list->mNumberBuffers = static_cast<UInt32>(bufferSamples.size());
  for (size_t index = 0; index < bufferSamples.size(); ++index) {
    list->mBuffers[index].mNumberChannels = 1;
    list->mBuffers[index].mDataByteSize =
        static_cast<UInt32>(bufferSamples[index] * sizeof(Float32));
    list->mBuffers[index].mData = result.buffers[index].data();
  }

  AudioTimeStamp timestamp{};
  AudioBufferList input{};
  input.mNumberBuffers = 0;
  result.status = ioProc.proc(ioProc.deviceID, &timestamp, &input, &timestamp,
                              list, &timestamp, ioProc.clientData);
  std::free(list);
  return result;
}

FakeAudioHardware::IOResult FakeAudioHardware::InvokeCurrentIOProc(
    AudioObjectID deviceID,
    const std::vector<size_t>& bufferSamples,
    Float32 initialValue) {
  IOProc ioProc{};
  {
    std::lock_guard lock(mutex_);
    const auto found =
        std::find_if(ioProcs_.rbegin(), ioProcs_.rend(),
                     [&](const IOProc& item) {
                       return item.deviceID == deviceID && !item.destroyed;
                     });
    if (found == ioProcs_.rend()) {
      throw std::runtime_error("no current fake IOProc");
    }
    ioProc = *found;
  }
  return Invoke(ioProc, bufferSamples, initialValue);
}

FakeAudioHardware::IOResult FakeAudioHardware::InvokeLastDestroyedIOProc(
    AudioObjectID deviceID,
    const std::vector<size_t>& bufferSamples,
    Float32 initialValue) {
  IOProc ioProc{};
  {
    std::lock_guard lock(mutex_);
    if (!lastDestroyedIOProc_ ||
        lastDestroyedIOProc_->deviceID != deviceID) {
      throw std::runtime_error("no destroyed fake IOProc");
    }
    ioProc = *lastDestroyedIOProc_;
  }
  return Invoke(ioProc, bufferSamples, initialValue);
}

ManualDispatch* ManualDispatch::active_ = nullptr;

ManualDispatch::ManualDispatch() {
  if (active_) {
    throw std::runtime_error("only one ManualDispatch may be active");
  }
  active_ = this;
}

ManualDispatch::~ManualDispatch() {
  std::lock_guard lock(mutex_);
  for (auto block : pending_) {
    Block_release(block);
  }
  for (auto block : delayed_) {
    Block_release(block);
  }
  active_ = nullptr;
}

ManualDispatch& ManualDispatch::Active() {
  if (!active_) {
    throw std::runtime_error("no ManualDispatch is active");
  }
  return *active_;
}

void ManualDispatch::Enqueue(dispatch_block_t block, bool delayed) {
  std::lock_guard lock(mutex_);
  (delayed ? delayed_ : pending_).push_back(Block_copy(block));
}

void ManualDispatch::DrainOne() {
  dispatch_block_t block = nullptr;
  {
    std::lock_guard lock(mutex_);
    if (pending_.empty()) {
      return;
    }
    block = pending_.front();
    pending_.pop_front();
  }
  block();
  Block_release(block);
}

void ManualDispatch::DrainAll() {
  while (Pending() != 0) {
    DrainOne();
  }
}

void ManualDispatch::DrainDelayed() {
  {
    std::lock_guard lock(mutex_);
    pending_.insert(pending_.end(), delayed_.begin(), delayed_.end());
    delayed_.clear();
  }
  DrainAll();
}

size_t ManualDispatch::Pending() const {
  std::lock_guard lock(mutex_);
  return pending_.size();
}

size_t ManualDispatch::PendingDelayed() const {
  std::lock_guard lock(mutex_);
  return delayed_.size();
}

void SetFakeHostTime(UInt64 value) {
  gHostTime.store(value);
}

void AdvanceFakeHostTime(UInt64 amount) {
  gHostTime.fetch_add(amount);
}

FakePluginHost* FakePluginHost::active_ = nullptr;

FakePluginHost::FakePluginHost() {
  if (active_) {
    throw std::runtime_error("only one FakePluginHost may be active");
  }
  active_ = this;
  interface_.PropertiesChanged = &PropertiesChanged;
  interface_.CopyFromStorage = &CopyFromStorage;
  interface_.WriteToStorage = &WriteToStorage;
  interface_.DeleteFromStorage = &DeleteFromStorage;
  interface_.RequestDeviceConfigurationChange =
      &RequestDeviceConfigurationChange;
}

FakePluginHost::~FakePluginHost() {
  active_ = nullptr;
}

FakePluginHost& FakePluginHost::Active() {
  if (!active_) {
    throw std::runtime_error("no FakePluginHost is active");
  }
  return *active_;
}

AudioServerPlugInHostRef FakePluginHost::Ref() {
  return &interface_;
}

size_t FakePluginHost::NotificationCount() const {
  return notifications_.size();
}

size_t FakePluginHost::NotificationCount(AudioObjectID objectID) const {
  return std::count_if(notifications_.begin(), notifications_.end(),
                       [&](const Notification& notification) {
                         return notification.objectID == objectID;
                       });
}

const std::vector<FakePluginHost::Notification>&
FakePluginHost::Notifications() const {
  return notifications_;
}

const std::vector<std::pair<AudioObjectID, UInt64>>&
FakePluginHost::ConfigurationRequests() const {
  return configurationRequests_;
}

void FakePluginHost::CompleteConfigurationRequests(
    const std::shared_ptr<aspl::Plugin>& plugin) {
  while (completedConfigurationRequests_ < configurationRequests_.size()) {
    const auto [deviceID, action] =
        configurationRequests_[completedConfigurationRequests_++];
    const auto device = plugin->GetDeviceByID(deviceID);
    if (!device) {
      throw std::runtime_error(
          "configuration request references an unknown device");
    }
    const auto status =
        device->PerformConfigurationChange(deviceID, action, nullptr);
    if (status != noErr) {
      throw std::runtime_error("configuration request failed");
    }
  }
}

OSStatus FakePluginHost::PropertiesChanged(
    AudioServerPlugInHostRef host,
    AudioObjectID objectID,
    UInt32 addressCount,
    const AudioObjectPropertyAddress* addresses) {
  Active().notifications_.push_back(
      {objectID, {addresses, addresses + addressCount}});
  return noErr;
}

OSStatus FakePluginHost::CopyFromStorage(AudioServerPlugInHostRef host,
                                         CFStringRef key,
                                         CFPropertyListRef* data) {
  *data = nullptr;
  return noErr;
}

OSStatus FakePluginHost::WriteToStorage(AudioServerPlugInHostRef host,
                                        CFStringRef key,
                                        CFPropertyListRef data) {
  return noErr;
}

OSStatus FakePluginHost::DeleteFromStorage(AudioServerPlugInHostRef host,
                                           CFStringRef key) {
  return noErr;
}

OSStatus FakePluginHost::RequestDeviceConfigurationChange(
    AudioServerPlugInHostRef host,
    AudioObjectID deviceID,
    UInt64 action,
    void* info) {
  Active().configurationRequests_.push_back({deviceID, action});
  return noErr;
}

bool ProxyDeviceTestAccess::IsRunning(const ProxyDevice& device) {
  return device.targetIORunning_.load();
}

UInt64 ProxyDeviceTestAccess::Generation(const ProxyDevice& device) {
  return device.targetIOGeneration_.load();
}

UInt64 ProxyDeviceTestAccess::FramesRead(const ProxyDevice& device) {
  return device.totalFramesRead_.load();
}

UInt64 ProxyDeviceTestAccess::Underruns(const ProxyDevice& device) {
  return device.underrunCount_;
}

UInt64 ProxyDeviceTestAccess::IOProcCalls(const ProxyDevice& device) {
  return device.ioProcCallCount_;
}

UInt64 ProxyDeviceTestAccess::OverrunSamples(const ProxyDevice& device) {
  return device.overrunSampleCount_.load();
}

UInt64 ProxyDeviceTestAccess::WriteCalls(const ProxyDevice& device) {
  return device.writeCallCount_.load();
}

UInt64 ProxyDeviceTestAccess::WriteSamples(const ProxyDevice& device) {
  return device.writeSampleCount_.load();
}

UInt32 ProxyDeviceTestAccess::MaxWriteChunk(const ProxyDevice& device) {
  return device.maxWriteChunkSamples_.load();
}

size_t ProxyDeviceTestAccess::RingCount(const ProxyDevice& device) {
  return device.ringBuffer_ ? device.ringBuffer_->GetCount() : 0;
}

size_t ProxyDeviceTestAccess::RingCapacity(const ProxyDevice& device) {
  return device.ringBuffer_ ? device.ringBuffer_->GetCapacity() : 0;
}

size_t ProxyDeviceTestAccess::RingFillMinimum(const ProxyDevice& device) {
  return device.ringFillMin_;
}

size_t ProxyDeviceTestAccess::RingFillMaximum(const ProxyDevice& device) {
  return device.ringFillMax_;
}

ZeroTimestamp ProxyDeviceTestAccess::Timestamp(const ProxyDevice& device) {
  return device.zts_.load();
}

OSStatus ProxyDeviceTestAccess::ApplyRate(ProxyDevice& device, Float64 rate) {
  return device.ApplySampleRateChange(rate);
}

ProxyAudioScenario::ProxyAudioScenario()
    : context(std::make_shared<aspl::Context>(
          std::make_shared<ProxyAudio::Tracer>(aspl::Tracer::Mode::Noop))) {
  SetFakeHostTime(0);
  hardware.SetVector<AudioObjectID>(
      kAudioObjectSystemObject,
      Address(kAudioHardwarePropertyDevices), {});
}

ProxyAudioScenario::~ProxyAudioScenario() = default;

AudioObjectID ProxyAudioScenario::AddDevice(const std::string& alias,
                                             DeviceSpec spec) {
  if (spec.uid.empty()) {
    spec.uid = "test." + alias;
  }
  IDs ids;
  ids.device = nextID_++;
  if (spec.hasOutputStream) {
    ids.stream = nextID_++;
  }
  if (spec.hasVolume) {
    ids.volume = nextID_++;
  }
  if (spec.hasMute) {
    ids.mute = nextID_++;
  }
  ids_[alias] = ids;

  hardware.Set(ids.device, Address(kAudioObjectPropertyClass), spec.classID);
  hardware.SetString(ids.device, Address(kAudioObjectPropertyName), spec.name);
  hardware.SetString(ids.device, Address(kAudioDevicePropertyDeviceUID),
                     spec.uid);
  hardware.SetString(ids.device, Address(kAudioDevicePropertyModelUID),
                     spec.uid + ".model");
  hardware.Set<UInt32>(
      ids.device,
      Address(kAudioDevicePropertyDeviceCanBeDefaultDevice,
              kAudioObjectPropertyScopeOutput),
      1);
  hardware.Set<UInt32>(
      ids.device,
      Address(kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
              kAudioObjectPropertyScopeOutput),
      1);
  hardware.Set(ids.device, Address(kAudioDevicePropertyNominalSampleRate),
               spec.sampleRate);
  hardware.SetVector<AudioValueRange>(
      ids.device, Address(kAudioDevicePropertyAvailableNominalSampleRates),
      {{spec.sampleRate, spec.sampleRate}, {44100, 44100}, {96000, 96000}});
  hardware.Set<UInt32>(
      ids.device,
      Address(kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput),
      spec.latency);

  std::vector<AudioObjectID> streams;
  if (ids.stream != kAudioObjectUnknown) {
    streams.push_back(ids.stream);
    hardware.Set(ids.stream, Address(kAudioObjectPropertyClass),
                 kAudioStreamClassID);
    hardware.Set<UInt32>(ids.stream, Address(kAudioStreamPropertyDirection), 0);
    hardware.Set<UInt32>(ids.stream,
                         Address(kAudioStreamPropertyStartingChannel), 1);
    hardware.Set(ids.stream, Address(kAudioStreamPropertyVirtualFormat),
                 Float32Format(spec.sampleRate, spec.channels));
    hardware.Set<UInt32>(ids.stream, Address(kAudioStreamPropertyLatency),
                         spec.streamLatency);
  }
  hardware.SetVector(ids.device,
                     Address(kAudioDevicePropertyStreams,
                             kAudioObjectPropertyScopeOutput),
                     streams);

  std::vector<AudioObjectID> controls;
  if (ids.volume != kAudioObjectUnknown) {
    controls.push_back(ids.volume);
    hardware.Set(ids.volume, Address(kAudioObjectPropertyClass),
                 kAudioVolumeControlClassID);
    hardware.Set(ids.volume, Address(kAudioControlPropertyScope),
                 kAudioObjectPropertyScopeOutput);
    hardware.Set(ids.volume, Address(kAudioControlPropertyElement),
                 kAudioObjectPropertyElementMain);
    hardware.Set(ids.volume, Address(kAudioLevelControlPropertyDecibelRange),
                 AudioValueRange{spec.minimumDecibels, 0});
    hardware.Set(ids.volume, Address(kAudioLevelControlPropertyScalarValue),
                 spec.volume);
  }
  if (ids.mute != kAudioObjectUnknown) {
    controls.push_back(ids.mute);
    hardware.Set(ids.mute, Address(kAudioObjectPropertyClass),
                 kAudioMuteControlClassID);
    hardware.Set(ids.mute, Address(kAudioControlPropertyScope),
                 kAudioObjectPropertyScopeOutput);
    hardware.Set(ids.mute, Address(kAudioControlPropertyElement),
                 kAudioObjectPropertyElementMain);
    hardware.Set(ids.mute, Address(kAudioBooleanControlPropertyValue),
                 spec.muted);
  }
  hardware.SetVector(ids.device,
                     Address(kAudioObjectPropertyControlList,
                             kAudioObjectPropertyScopeOutput),
                     controls);
  return ids.device;
}

AudioObjectID ProxyAudioScenario::DeviceID(const std::string& alias) const {
  return ids_.at(alias).device;
}

AudioObjectID ProxyAudioScenario::StreamID(const std::string& alias) const {
  return ids_.at(alias).stream;
}

AudioObjectID ProxyAudioScenario::VolumeID(const std::string& alias) const {
  return ids_.at(alias).volume;
}

AudioObjectID ProxyAudioScenario::MuteID(const std::string& alias) const {
  return ids_.at(alias).mute;
}

void ProxyAudioScenario::SetSystemDevices(
    const std::vector<std::string>& aliases) {
  std::vector<AudioObjectID> values;
  values.reserve(aliases.size());
  for (const auto& alias : aliases) {
    values.push_back(DeviceID(alias));
  }
  hardware.SetVector(kAudioObjectSystemObject,
                     Address(kAudioHardwarePropertyDevices), values);
}

std::shared_ptr<ProxyDevice> ProxyAudioScenario::CreateProxy(
    const std::string& alias) {
  auto proxy = ProxyDevice::Create(DeviceID(alias), context);
  proxies_[alias] = proxy;
  return proxy;
}

std::shared_ptr<ProxyDevice> ProxyAudioScenario::Proxy(
    const std::string& alias) const {
  return proxies_.at(alias);
}

void ProxyAudioScenario::SetTargetVolume(const std::string& alias,
                                         Float32 value,
                                         bool notify) {
  hardware.Set(VolumeID(alias), Address(kAudioLevelControlPropertyScalarValue),
               value);
  if (notify) {
    hardware.Notify(VolumeID(alias),
                    Address(kAudioLevelControlPropertyScalarValue));
  }
}

Float32 ProxyAudioScenario::TargetVolume(const std::string& alias) const {
  return hardware.Value<Float32>(
      VolumeID(alias), Address(kAudioLevelControlPropertyScalarValue));
}

void ProxyAudioScenario::Write(const std::string& alias,
                               const std::vector<Float32>& samples) {
  proxies_.at(alias)->OnWriteMixedOutput(
      proxies_.at(alias)->GetStreamByIndex(aspl::Direction::Output, 0), 0, 0,
      samples.data(), static_cast<UInt32>(samples.size() * sizeof(Float32)));
}

FakeAudioHardware::IOResult ProxyAudioScenario::ReadTarget(
    const std::string& alias,
    const std::vector<size_t>& bufferSamples,
    Float32 initialValue) {
  return hardware.InvokeCurrentIOProc(DeviceID(alias), bufferSamples,
                                      initialValue);
}

void ProxyAudioScenario::Start(const std::string& alias) {
  const auto status = proxies_.at(alias)->OnStartIO();
  if (status != noErr) {
    throw std::runtime_error("scenario failed to start proxy I/O");
  }
}

void ProxyAudioScenario::Stop(const std::string& alias) {
  proxies_.at(alias)->OnStopIO();
}

}  // namespace ProxyAudio::Test

namespace ProxyAudio {

UInt64 CurrentHostTime() {
  return Test::gHostTime.load();
}

void DispatchAsync(dispatch_block_t block) {
  Test::ManualDispatch::Active().Enqueue(block, false);
}

void DispatchAfter(dispatch_block_t block, const DispatchTime& time) {
  Test::ManualDispatch::Active().Enqueue(block, true);
}

}  // namespace ProxyAudio

extern "C" {

OSStatus AudioObjectGetPropertyDataSize(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32* outDataSize) {
  return ProxyAudio::Test::FakeAudioHardware::Active().GetPropertyDataSize(
      objectID, *address, outDataSize);
}

OSStatus AudioObjectGetPropertyData(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32* ioDataSize,
    void* outData) {
  return ProxyAudio::Test::FakeAudioHardware::Active().GetPropertyData(
      objectID, *address, ioDataSize, outData);
}

OSStatus AudioObjectSetPropertyData(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress* address,
    UInt32 qualifierDataSize,
    const void* qualifierData,
    UInt32 dataSize,
    const void* data) {
  return ProxyAudio::Test::FakeAudioHardware::Active().SetPropertyData(
      objectID, *address, dataSize, data);
}

OSStatus AudioObjectAddPropertyListener(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress* address,
    AudioObjectPropertyListenerProc listener,
    void* clientData) {
  return ProxyAudio::Test::FakeAudioHardware::Active().AddListener(
      objectID, *address, listener, clientData);
}

OSStatus AudioObjectRemovePropertyListener(
    AudioObjectID objectID,
    const AudioObjectPropertyAddress* address,
    AudioObjectPropertyListenerProc listener,
    void* clientData) {
  return ProxyAudio::Test::FakeAudioHardware::Active().RemoveListener(
      objectID, *address, listener, clientData);
}

OSStatus AudioDeviceCreateIOProcID(AudioObjectID deviceID,
                                   AudioDeviceIOProc proc,
                                   void* clientData,
                                   AudioDeviceIOProcID* outID) {
  return ProxyAudio::Test::FakeAudioHardware::Active().CreateIOProc(
      deviceID, proc, clientData, outID);
}

OSStatus AudioDeviceStart(AudioObjectID deviceID, AudioDeviceIOProcID id) {
  return ProxyAudio::Test::FakeAudioHardware::Active().StartIOProc(deviceID,
                                                                   id);
}

OSStatus AudioDeviceStop(AudioObjectID deviceID, AudioDeviceIOProcID id) {
  return ProxyAudio::Test::FakeAudioHardware::Active().StopIOProc(deviceID,
                                                                  id);
}

OSStatus AudioDeviceDestroyIOProcID(AudioObjectID deviceID,
                                    AudioDeviceIOProcID id) {
  return ProxyAudio::Test::FakeAudioHardware::Active().DestroyIOProc(deviceID,
                                                                     id);
}

}  // extern "C"
