// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>

#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

#include "Utils.hpp"

namespace ProxyAudio {

#define EXPECT(condition, exception) \
  if (!(condition)) {                \
    throw exception;                 \
  }

class ErrorWithCode : public std::exception {
 public:
  ErrorWithCode(const uint32_t& code, const std::string& message = "")
      : code_(code), message_(FormatMessage(message)) {}

  const char* what() const noexcept override { return message_.c_str(); }

  uint32_t code() const noexcept { return code_; }

  void log() const noexcept { Log("%{public}s", what()); }

 private:
  std::string FormatMessage(const std::string& message) const {
    std::stringstream ss;

    ss << message << ((message.length() > 0U) ? " " : "") << "("
       << "0x" << std::hex << std::uppercase << code_ << ")";

    return ss.str();
  }

  uint32_t code_;
  std::string message_;
};

// Error thrown when the data size is not enough for the property.
class BadDataSizeError : public ErrorWithCode {
 public:
  BadDataSizeError(
      const std::string& message = "Not enough space for return data.")
      : ErrorWithCode(kAudioHardwareBadPropertySizeError, message) {}
};

}  // namespace ProxyAudio
