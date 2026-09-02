// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

// DriverHandler intentionally remains private production wiring. Including the
// source here keeps the production file untouched while making that wiring
// testable in this one translation unit.
#include "../src/Driver.cpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ProxyVolumeControl.hpp"
#include "TestSupport.hpp"

namespace ProxyAudio::Test {
namespace {

std::shared_ptr<ProxyDevice> FindProxyForTarget(
    const std::shared_ptr<aspl::Plugin>& plugin,
    AudioObjectID targetID) {
  for (UInt32 index = 0; index < plugin->GetDeviceCount(); ++index) {
    auto proxy =
        std::dynamic_pointer_cast<ProxyDevice>(plugin->GetDeviceByIndex(index));
    if (proxy && proxy->GetTargetObjectID() == targetID) {
      return proxy;
    }
  }
  return nullptr;
}

TEST(HealthyBaselineScenarioTest, ReconcilesAndRunsTheSuppliedLogPattern) {
  ProxyAudioScenario scenario;
  auto plugin = std::make_shared<aspl::Plugin>(scenario.context);

  for (const auto* alias : {"stale-a", "stale-b", "stale-c"}) {
    scenario.AddDevice(alias);
    auto stale = scenario.CreateProxy(alias);
    ASSERT_NE(stale, nullptr);
    plugin->AddDevice(stale, false);
  }
  ASSERT_EQ(plugin->GetDeviceCount(), 3u);
  scenario.SetSystemDevices({});

  FakePluginHost host;
  scenario.context->Host = host.Ref();
  {
    DriverHandler handler(scenario.context, plugin);
    ASSERT_EQ(handler.OnInitialize(), noErr);
    ASSERT_EQ(scenario.dispatch.Pending(), 1u);
    scenario.dispatch.DrainAll();

    EXPECT_EQ(plugin->GetDeviceCount(), 0u);
    ASSERT_EQ(host.NotificationCount(plugin->GetID()), 1u);
    ASSERT_EQ(host.Notifications().back().addresses.size(), 2u);

    DeviceSpec speaker;
    speaker.name = "Speaker";
    speaker.uid = "anonymous.speaker";
    speaker.latency = 70;
    speaker.volume = 0.121945f;
    scenario.AddDevice("speaker", speaker);
    DeviceSpec headphones;
    headphones.name = "Headphones";
    scenario.AddDevice("headphones", headphones);
    DeviceSpec display;
    display.name = "Display";
    scenario.AddDevice("display", display);
    DeviceSpec inputOnly;
    inputOnly.name = "Input only";
    inputOnly.hasOutputStream = false;
    inputOnly.hasVolume = false;
    inputOnly.hasMute = false;
    scenario.AddDevice("input-only", inputOnly);
    scenario.SetSystemDevices(
        {"speaker", "headphones", "display", "input-only"});
    scenario.hardware.Notify(kAudioObjectSystemObject,
                             Address(kAudioHardwarePropertyDevices));
    scenario.dispatch.DrainAll();

    ASSERT_EQ(plugin->GetDeviceCount(), 3u);
    EXPECT_EQ(host.NotificationCount(plugin->GetID()), 2u)
        << "device additions should produce one consolidated notification";

    std::vector<AudioObjectID> publishedDevices{
        scenario.DeviceID("speaker"), scenario.DeviceID("headphones"),
        scenario.DeviceID("display"), scenario.DeviceID("input-only")};
    for (UInt32 index = 0; index < plugin->GetDeviceCount(); ++index) {
      const auto proxy = plugin->GetDeviceByIndex(index);
      publishedDevices.push_back(proxy->GetID());
      scenario.hardware.SetString(proxy->GetID(),
                                  Address(kAudioObjectPropertyName),
                                  proxy->GetName());
      scenario.hardware.SetString(proxy->GetID(),
                                  Address(kAudioDevicePropertyDeviceUID),
                                  proxy->GetDeviceUID());
    }
    scenario.hardware.SetVector<AudioObjectID>(
        kAudioObjectSystemObject, Address(kAudioHardwarePropertyDevices),
        publishedDevices);
    scenario.hardware.Notify(kAudioObjectSystemObject,
                             Address(kAudioHardwarePropertyDevices));
    scenario.dispatch.DrainAll();

    EXPECT_EQ(plugin->GetDeviceCount(), 3u);
    EXPECT_EQ(host.NotificationCount(plugin->GetID()), 2u)
        << "an idempotent rescan must not duplicate or notify";

    auto proxy = FindProxyForTarget(plugin, scenario.DeviceID("speaker"));
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->GetLatency(), 8262u);
    auto volume = std::dynamic_pointer_cast<ProxyVolumeControl>(
        proxy->GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0));
    ASSERT_NE(volume, nullptr);
    EXPECT_NEAR(volume->GetScalarValue(), 0.121945f, 1e-5f);

    const auto runSession = [&]() {
      EXPECT_EQ(ProxyDeviceTestAccess::FramesRead(*proxy), 0u);
      EXPECT_EQ(ProxyDeviceTestAccess::Underruns(*proxy), 0u);
      EXPECT_EQ(ProxyDeviceTestAccess::OverrunSamples(*proxy), 0u);
      ASSERT_EQ(proxy->OnStartIO(), noErr);
      scenario.dispatch.DrainAll();

      const auto startup = scenario.hardware.InvokeCurrentIOProc(
          scenario.DeviceID("speaker"), {4}, 1.0f);
      EXPECT_EQ(startup.buffers[0], std::vector<Float32>(4, 0.0f));

      const std::vector<Float32> samples{0.1f, -0.1f, 0.2f, -0.2f,
                                         0.3f, -0.3f, 0.4f, -0.4f};
      proxy->OnWriteMixedOutput(
          proxy->GetStreamByIndex(aspl::Direction::Output, 0), 0, 0,
          samples.data(),
          static_cast<UInt32>(samples.size() * sizeof(Float32)));
      const auto playback = scenario.hardware.InvokeCurrentIOProc(
          scenario.DeviceID("speaker"), {3, 5});
      EXPECT_EQ(playback.buffers[0],
                (std::vector<Float32>{0.1f, -0.1f, 0.2f}));
      EXPECT_EQ(playback.buffers[1],
                (std::vector<Float32>{-0.2f, 0.3f, -0.3f, 0.4f, -0.4f}));
      EXPECT_GT(ProxyDeviceTestAccess::Underruns(*proxy), 0u);
      EXPECT_EQ(ProxyDeviceTestAccess::OverrunSamples(*proxy), 0u);

      proxy->OnStopIO();
      const auto late = scenario.hardware.InvokeLastDestroyedIOProc(
          scenario.DeviceID("speaker"), {4}, 1.0f);
      EXPECT_EQ(late.buffers[0], std::vector<Float32>(4, 0.0f));
      EXPECT_EQ(ProxyDeviceTestAccess::FramesRead(*proxy), 0u);
      EXPECT_EQ(ProxyDeviceTestAccess::Underruns(*proxy), 0u);
      EXPECT_EQ(ProxyDeviceTestAccess::OverrunSamples(*proxy), 0u);
    };

    runSession();

    for (const auto value : {0.183673f, 0.258000f, 0.308642f}) {
      ASSERT_EQ(volume->SetScalarValue(value), noErr);
      EXPECT_NEAR(scenario.hardware.Value<Float32>(
                      scenario.VolumeID("speaker"),
                      Address(kAudioLevelControlPropertyScalarValue)),
                  value, 1e-5f);
    }
    for (const auto value : {0.258000f, 0.183673f}) {
      scenario.SetTargetVolume("speaker", value);
      EXPECT_NEAR(volume->GetScalarValue(), value, 1e-5f);
    }

    runSession();

    EXPECT_EQ(scenario.hardware.CallCount(
                  FakeAudioHardware::Operation::CreateIOProc),
              2u);
    EXPECT_EQ(scenario.hardware.CallCount(
                  FakeAudioHardware::Operation::StartIOProc),
              2u);
    EXPECT_EQ(scenario.hardware.CallCount(
                  FakeAudioHardware::Operation::StopIOProc),
              2u);
    EXPECT_EQ(scenario.hardware.CallCount(
                  FakeAudioHardware::Operation::DestroyIOProc),
              2u);
  }

  scenario.context->Host = nullptr;
}

TEST(DriverReconciliationTest, MalformedDeviceDoesNotBlockEligibleDevices) {
  ProxyAudioScenario scenario;
  auto plugin = std::make_shared<aspl::Plugin>(scenario.context);
  scenario.AddDevice("valid");
  scenario.AddDevice("malformed");
  scenario.hardware.Erase(
      scenario.DeviceID("malformed"),
      Address(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput));
  scenario.SetSystemDevices({"malformed", "valid"});

  FakePluginHost host;
  scenario.context->Host = host.Ref();
  {
    DriverHandler handler(scenario.context, plugin);
    ASSERT_EQ(handler.OnInitialize(), noErr);
    scenario.dispatch.DrainAll();
    ASSERT_EQ(plugin->GetDeviceCount(), 1u);
    EXPECT_EQ(std::dynamic_pointer_cast<ProxyDevice>(
                  plugin->GetDeviceByIndex(0))
                  ->GetTargetObjectID(),
              scenario.DeviceID("valid"));
    EXPECT_EQ(host.NotificationCount(plugin->GetID()), 1u);
  }
  scenario.context->Host = nullptr;
}

}  // namespace
}  // namespace ProxyAudio::Test
