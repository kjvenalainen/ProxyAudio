// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <aspl/Tracer.hpp>
#include <sstream>
#include <string>

namespace ProxyAudio {

struct Tracer : public aspl::Tracer {
  static constexpr size_t DepthSoftLimit = 10;

  explicit Tracer(Mode mode = Mode::Syslog, Style style = Style::Hierarchical)
      : aspl::Tracer(mode, style), mode_(mode), style_(style) {}

  inline std::string FormatMessage(const char* message, UInt32 depth) override {
    if (depth > DepthSoftLimit) {
      depth = DepthSoftLimit;
    }

    std::ostringstream ss;
    ss << "[PROXY AUDIO] ";

    if (style_ == Style::Hierarchical) {
      ss << "|";
      for (UInt32 i = 0; i <= depth; i++) {
        ss << "-";
      }
      ss << " ";
    }

    ss << message;

    return ss.str();
  }

 private:
  Mode mode_;
  Style style_;
};

}  // namespace ProxyAudio
