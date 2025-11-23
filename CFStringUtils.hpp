// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreFoundation/CFString.h>

#include <string>

namespace ProxyAudio {

// Convert std::string to CFStringRef (caller must CFRelease the result)
inline CFStringRef StringToCFString(const std::string& str) {
  return CFStringCreateWithCString(nullptr, str.c_str(), kCFStringEncodingUTF8);
}

// Convert CFStringRef to std::string
inline std::string CFStringToString(CFStringRef cfStr) {
  if (cfStr == nullptr) {
    return "";
  }
  CFIndex length = CFStringGetLength(cfStr);
  CFIndex maxSize =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(maxSize, '\0');
  Boolean success =
      CFStringGetCString(cfStr, &result[0], maxSize, kCFStringEncodingUTF8);
  if (success) {
    result.resize(strlen(result.c_str()));
  } else {
    result.clear();
  }
  return result;
}

}  // namespace ProxyAudio
