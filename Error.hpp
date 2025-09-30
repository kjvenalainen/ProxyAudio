// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <cstdint>
#include <exception>
#include <string>

namespace ProxyAudio {

class ErrorWithCode : public std::exception {
 public:
  ErrorWithCode(const uint32_t& code, const std::string& message = "")
      : code_(code), message_(FormatMessage(message)) {}

  const char* what() const noexcept override { return message_.c_str(); }

  uint32_t code() const noexcept { return code_; }

 private:
  std::string FormatMessage(const std::string& message) const {
    return message + ((message.length() > 0U) ? " " : "") + "(" +
           std::to_string(code_) + ")";
  }

  uint32_t code_;
  std::string message_;
};

}  // namespace ProxyAudio
