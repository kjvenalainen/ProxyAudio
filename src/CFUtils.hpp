// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace ProxyAudio {

// Get a std::string from a CFStringRef.
inline std::string StringFromCFStringRef(const CFStringRef& cfstr) noexcept {
  if (!cfstr) {
    return "NULL";
  }

  const char* ptr = CFStringGetCStringPtr(cfstr, kCFStringEncodingUTF8);
  if (ptr) {
    std::string result(ptr);
    return result;
  }

  CFIndex length = CFStringGetLength(cfstr) + 1;
  CFIndex maxSize =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::vector<char> buffer(maxSize);
  if (CFStringGetCString(cfstr, buffer.data(), maxSize,
                         kCFStringEncodingUTF8)) {
    return std::string(buffer.data());
  }

  return "FAILED";
}

// Converts a FourCC to a 4 character string.
inline std::string FourCC(const UInt32& code) noexcept {
  char buffer[4];
  buffer[0] = (code >> 24) & 0xFF;
  buffer[1] = (code >> 16) & 0xFF;
  buffer[2] = (code >> 8) & 0xFF;
  buffer[3] = code & 0xFF;
  return std::string(buffer, 4);
}

// Converts the first 4 characters of a string to a FourCC.
inline UInt32 FourCC(const std::string& code) noexcept {
  const char* view = code.c_str();
  return view[0] << 24 | view[1] << 16 | view[2] << 8 | view[3];
}

inline std::string ToString(
    const AudioObjectPropertyAddress& address) noexcept {
  return std::string(FourCC(address.mSelector)) + ":" + FourCC(address.mScope) +
         ":" +
         (address.mElement == kAudioObjectPropertyElementMain
              ? "main"
              : FourCC(address.mElement));
}

template <typename T>
inline std::string ToString(const T& value) noexcept {
  return std::to_string(value);
}

template <>
inline std::string ToString(const AudioStreamBasicDescription& value) noexcept {
  std::stringstream ss;

  ss << "{id: " << FourCC(value.mFormatID) << ", ";
  ss << "flags: 0x" << std::hex << value.mFormatFlags << std::dec << ", ";
  ss << "rate: " << value.mSampleRate << ", ";
  ss << "bits: " << value.mBitsPerChannel << ", ";
  ss << "chans: " << value.mChannelsPerFrame << ", ";
  ss << "frames/pkt: " << value.mFramesPerPacket << ", ";
  ss << "bytes/frame: " << value.mBytesPerFrame << ", ";
  ss << "bytes/pkt: " << value.mBytesPerPacket << "}";

  return ss.str();
}

template <>
inline std::string ToString(const AudioValueRange& value) noexcept {
  return "[" + std::to_string(value.mMinimum) + " - " +
         std::to_string(value.mMaximum) + "]";
}

template <typename E>
inline std::string ToString(const std::vector<E>& value) noexcept {
  std::stringstream ss;
  ss << "(" << value.size() << ") [";
  for (const auto& range : value) {
    ss << ToString(range) << ", ";
  }
  ss << "]";
  return ss.str();
}

inline bool operator==(const AudioStreamBasicDescription& a,
                       const AudioStreamBasicDescription& b) noexcept {
  return a.mFormatID == b.mFormatID && a.mFormatFlags == b.mFormatFlags &&
         a.mSampleRate == b.mSampleRate &&
         a.mBitsPerChannel == b.mBitsPerChannel &&
         a.mChannelsPerFrame == b.mChannelsPerFrame &&
         a.mFramesPerPacket == b.mFramesPerPacket &&
         a.mBytesPerFrame == b.mBytesPerFrame &&
         a.mBytesPerPacket == b.mBytesPerPacket;
}

inline bool operator!=(const AudioStreamBasicDescription& a,
                       const AudioStreamBasicDescription& b) noexcept {
  return !(a == b);
}

// Auto-release wrapper for a CFTypeRef object.
template <typename T,
          typename std::enable_if_t<std::is_convertible_v<T, CFTypeRef>, int> =
              0>
class CFAutoRef {
 public:
  CFAutoRef() noexcept : ref_(nullptr) {}

  // Create a CFNumberRef, intenally calling CFNumberCreate.
  template <typename TT = T,
            std::enable_if_t<std::is_same_v<TT, CFNumberRef>, int> = 0>
  CFAutoRef(CFAllocatorRef allocator,
            CFNumberType theType,
            const void* valuePtr) noexcept
      : ref_(CFNumberCreate(allocator, theType, valuePtr)) {}

  // Create a CFStringRef.
  template <typename TT = T,
            std::enable_if_t<std::is_same_v<TT, CFStringRef>, int> = 0>
  CFAutoRef(CFAllocatorRef allocator, const std::string& str) noexcept
      : ref_(CFStringCreateWithCString(allocator,
                                       str.c_str(),
                                       kCFStringEncodingUTF8)) {}

  // No copy.
  CFAutoRef(const CFAutoRef&) = delete;

  // No copy.
  CFAutoRef& operator=(const CFAutoRef&) = delete;

  // Move OK.
  CFAutoRef(CFAutoRef&& other) noexcept {
    ref_ = other.ref_;
    other.ref_ = nullptr;
  };

  // Move OK.
  CFAutoRef& operator=(CFAutoRef&& other) noexcept {
    if (*this != other) {
      if (ref_ != nullptr) {
        CFRelease(ref_);
      }
      ref_ = other.ref_;
      other.ref_ = nullptr;
    }
    return *this;
  }

  // Release the owned CFTypeRef.
  ~CFAutoRef() noexcept {
    if (ref_ != nullptr) {
      CFRelease(ref_);
    }
    ref_ = nullptr;
  }

  // Return a reference to the underlying CFTypeRef.
  T& operator*() noexcept { return ref_; }

  // Return a reference to the underlying CFTypeRef.
  T& operator*() const noexcept { return ref_; }

  // Behave like a direct CFTypeRef, returning the data pointer using the &
  // operator.
  T* operator&() noexcept { return &ref_; }

  // Behave like a direct CFTypeRef, returning the data pointer using the &
  // operator.
  T* operator&() const noexcept { return &ref_; }

 private:
  T ref_;
};

}  // namespace ProxyAudio
