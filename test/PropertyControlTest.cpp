// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include <gtest/gtest.h>

#include <array>
#include <memory>

#include "Error.hpp"
#include "ProxyMuteControl.hpp"
#include "ProxyProperty.hpp"
#include "ProxyVolumeControl.hpp"
#include "TestSupport.hpp"

namespace ProxyAudio::Test {
namespace {

TEST(ProxyPropertyTest, ReadsWritesUpdatesAndSuppressesUnchangedCallbacks) {
  ProxyAudioScenario scenario;
  constexpr AudioObjectID kObject = 42;
  const auto address = Address(kAudioDevicePropertyNominalSampleRate);
  scenario.hardware.Set<Float64>(kObject, address, 48000);

  Float64 mirroredValue = 48000;
  size_t setterCalls = 0;
  {
    ProxyProperty<Float64> property(
        kObject, scenario.context, address,
        [&](const Float64& value) {
          mirroredValue = value;
          ++setterCalls;
        },
        [&]() { return mirroredValue; });

    EXPECT_DOUBLE_EQ(property.GetValue(), 48000);
    property.SetValue(44100);
    EXPECT_DOUBLE_EQ(scenario.hardware.Value<Float64>(kObject, address),
                     44100);

    scenario.hardware.Notify(kObject, address);
    EXPECT_DOUBLE_EQ(mirroredValue, 44100);
    EXPECT_EQ(setterCalls, 1u);

    scenario.hardware.Notify(kObject, address);
    EXPECT_EQ(setterCalls, 1u) << "an unchanged target value was re-applied";
    EXPECT_EQ(scenario.hardware.ListenerCount(kObject, address), 1u);
  }

  EXPECT_EQ(scenario.hardware.ListenerCount(kObject, address), 0u);
}

TEST(ProxyPropertyTest, ReportsCoreAudioFailuresAndCleansUpBestEffort) {
  ProxyAudioScenario scenario;
  constexpr AudioObjectID kObject = 43;
  const auto address = Address(kAudioDevicePropertyNominalSampleRate);
  scenario.hardware.Set<Float64>(kObject, address, 48000);

  scenario.hardware.FailNext(FakeAudioHardware::Operation::AddListener,
                             kAudioHardwareUnspecifiedError, kObject, address);
  EXPECT_THROW(
      (ProxyProperty<Float64>(kObject, scenario.context, address,
                              [](const Float64&) {}, []() { return 0.0; })),
      OSStatusError);
  EXPECT_EQ(scenario.hardware.ListenerCount(), 0u);

  {
    ProxyProperty<Float64> property(
        kObject, scenario.context, address, [](const Float64&) {},
        []() { return 48000.0; });

    scenario.hardware.FailNext(FakeAudioHardware::Operation::GetSize,
                               kAudioHardwareBadObjectError, kObject, address);
    EXPECT_THROW(property.GetValue(), OSStatusError);

    scenario.hardware.FailNext(FakeAudioHardware::Operation::Set,
                               kAudioHardwareIllegalOperationError, kObject,
                               address);
    EXPECT_THROW(property.SetValue(96000), OSStatusError);

  }

  EXPECT_EQ(scenario.hardware.ListenerCount(kObject, address), 0u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::RemoveListener),
      1u);
}

TEST(ProxyControlsTest, VolumePropagatesBothWaysWithinCaptureTolerance) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  const auto controlID = scenario.VolumeID("speaker");
  const auto address = Address(kAudioLevelControlPropertyScalarValue);

  {
    ProxyVolumeControl control(controlID, scenario.context);
    EXPECT_NEAR(control.GetScalarValue(), 0.121945f, 1e-5f);

    for (const auto value : {0.183673f, 0.258000f, 0.308642f}) {
      ASSERT_EQ(control.SetScalarValue(value), noErr);
      EXPECT_NEAR(control.GetScalarValue(), value, 1e-5f);
      EXPECT_NEAR(scenario.hardware.Value<Float32>(controlID, address), value,
                  1e-5f);
    }

    scenario.hardware.Set<Float32>(controlID, address, 0.258000f);
    scenario.hardware.Notify(controlID, address);
    EXPECT_NEAR(control.GetScalarValue(), 0.258000f, 1e-5f);

    const auto setCalls =
        scenario.hardware.CallCount(FakeAudioHardware::Operation::Set);
    scenario.hardware.Notify(controlID, address);
    EXPECT_EQ(scenario.hardware.CallCount(FakeAudioHardware::Operation::Set),
              setCalls)
        << "an unchanged listener value should not be written back";

    std::array<Float32, 4> samples{0.75f, -0.75f, 0.5f, -0.5f};
    const auto original = samples;
    control.ApplyProcessing(samples.data(), 2, 2);
    EXPECT_EQ(samples, original)
        << "the proxy must not apply target volume processing twice";
  }

  EXPECT_EQ(scenario.hardware.ListenerCount(controlID, address), 0u);
}

TEST(ProxyControlsTest, MutePropagatesBothWaysWithoutDoubleProcessing) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  const auto controlID = scenario.MuteID("speaker");
  const auto address = Address(kAudioBooleanControlPropertyValue);

  ProxyMuteControl control(controlID, scenario.context);
  EXPECT_FALSE(control.GetIsMuted());
  ASSERT_EQ(control.SetIsMuted(true), noErr);
  EXPECT_EQ(scenario.hardware.Value<UInt32>(controlID, address), 1u);

  scenario.hardware.Set<UInt32>(controlID, address, 0);
  scenario.hardware.Notify(controlID, address);
  EXPECT_FALSE(control.GetIsMuted());

  std::array<Float32, 2> samples{0.5f, -0.5f};
  control.SetIsMuted(true);
  control.ApplyProcessing(samples.data(), 1, 2);
  EXPECT_EQ(samples, (std::array<Float32, 2>{0.5f, -0.5f}));
}

}  // namespace
}  // namespace ProxyAudio::Test
