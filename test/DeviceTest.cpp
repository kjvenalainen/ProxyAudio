// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include <gtest/gtest.h>

#include <array>
#include <memory>

#include "ProxyMuteControl.hpp"
#include "ProxyStream.hpp"
#include "ProxyVolumeControl.hpp"
#include "TestSupport.hpp"

namespace ProxyAudio::Test {
namespace {

TEST(ProxyDeviceTest, ClonesDeviceStreamAndRealControlParameters) {
  ProxyAudioScenario scenario;
  DeviceSpec spec;
  spec.name = "Desk Speaker";
  spec.uid = "anonymous.speaker";
  spec.sampleRate = 48000;
  spec.channels = 2;
  spec.latency = 70;
  spec.streamLatency = 19;
  scenario.AddDevice("speaker", spec);

  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  EXPECT_EQ(proxy->GetName(), "Desk Speaker (Proxy)");
  EXPECT_EQ(proxy->GetDeviceUID(), "anonymous.speaker_PA_Proxy");
  EXPECT_EQ(proxy->GetModelUID(), "anonymous.speaker.model_PA_Proxy");
  EXPECT_TRUE(proxy->GetCanBeDefaultDevice());
  EXPECT_TRUE(proxy->GetCanBeDefaultSystemDevice());
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 48000);
  EXPECT_EQ(proxy->GetLatency(), 8262u);
  EXPECT_EQ(proxy->GetZeroTimeStampPeriod(), 16384u);
  EXPECT_EQ(proxy->GetAvailableSampleRates().size(), 3u);

  ASSERT_EQ(proxy->GetStreamCount(aspl::Direction::Output), 1u);
  auto stream = std::dynamic_pointer_cast<ProxyStream>(
      proxy->GetStreamByIndex(aspl::Direction::Output, 0));
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->GetTargetObjectID(), scenario.StreamID("speaker"));
  EXPECT_EQ(stream->GetStartingChannel(), 1u);
  EXPECT_EQ(stream->GetLatency(), 19u);
  EXPECT_DOUBLE_EQ(stream->GetPhysicalFormat().mSampleRate, 48000);
  EXPECT_EQ(stream->GetPhysicalFormat().mChannelsPerFrame, 2u);
  EXPECT_DOUBLE_EQ(stream->GetVirtualFormat().mSampleRate, 48000);

  ASSERT_EQ(proxy->GetVolumeControlCount(kAudioObjectPropertyScopeOutput), 1u);
  ASSERT_EQ(proxy->GetMuteControlCount(kAudioObjectPropertyScopeOutput), 1u);
  EXPECT_NE(std::dynamic_pointer_cast<ProxyVolumeControl>(
                proxy->GetVolumeControlByIndex(
                    kAudioObjectPropertyScopeOutput, 0)),
            nullptr);
  EXPECT_NE(std::dynamic_pointer_cast<ProxyMuteControl>(
                proxy->GetMuteControlByIndex(kAudioObjectPropertyScopeOutput,
                                             0)),
            nullptr);
}

TEST(ProxyDeviceTest, AddsProcessingControlsOnlyWhenTargetControlsAreAbsent) {
  ProxyAudioScenario scenario;
  DeviceSpec spec;
  spec.hasVolume = false;
  spec.hasMute = false;
  scenario.AddDevice("digital", spec);

  auto proxy = scenario.CreateProxy("digital");
  ASSERT_NE(proxy, nullptr);
  auto volume = proxy->GetVolumeControlByIndex(
      kAudioObjectPropertyScopeOutput, 0);
  auto mute = proxy->GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0);
  ASSERT_NE(volume, nullptr);
  ASSERT_NE(mute, nullptr);
  EXPECT_EQ(std::dynamic_pointer_cast<ProxyVolumeControl>(volume), nullptr);
  EXPECT_EQ(std::dynamic_pointer_cast<ProxyMuteControl>(mute), nullptr);

  ASSERT_EQ(volume->SetScalarValue(0.5f), noErr);
  std::array<Float32, 2> samples{1.0f, -1.0f};
  proxy->GetStreamByIndex(aspl::Direction::Output, 0)
      ->ApplyProcessing(samples.data(), 1, 2);
  EXPECT_LT(samples[0], 1.0f);
  EXPECT_GT(samples[1], -1.0f);

  ASSERT_EQ(mute->SetIsMuted(true), noErr);
  proxy->GetStreamByIndex(aspl::Direction::Output, 0)
      ->ApplyProcessing(samples.data(), 1, 2);
  EXPECT_EQ(samples, (std::array<Float32, 2>{0.0f, 0.0f}));
}

TEST(ProxyDeviceTest, RejectsInvalidOrIncompleteTargetObjects) {
  ProxyAudioScenario scenario;
  DeviceSpec wrongClass;
  wrongClass.classID = kAudioStreamClassID;
  scenario.AddDevice("wrong-class", wrongClass);
  EXPECT_EQ(scenario.CreateProxy("wrong-class"), nullptr);

  scenario.AddDevice("missing-name");
  scenario.hardware.Erase(scenario.DeviceID("missing-name"),
                          Address(kAudioObjectPropertyName));
  EXPECT_EQ(scenario.CreateProxy("missing-name"), nullptr);
}

TEST(ProxyDeviceTest, RefreshesExistingStreamWhenIdentityIsStable) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  auto originalStream = proxy->GetStreamByIndex(aspl::Direction::Output, 0);

  auto updatedFormat = Float32Format(44100, 2);
  scenario.hardware.Set(scenario.StreamID("speaker"),
                        Address(kAudioStreamPropertyVirtualFormat),
                        updatedFormat);
  scenario.hardware.Set<UInt32>(scenario.StreamID("speaker"),
                                Address(kAudioStreamPropertyLatency), 27);

  const auto callsBeforeChange = scenario.hardware.Calls().size();
  ASSERT_EQ(ProxyDeviceTestAccess::ApplyRate(*proxy, 44100), noErr);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 44100);
  EXPECT_EQ(proxy->GetStreamByIndex(aspl::Direction::Output, 0),
            originalStream);
  EXPECT_DOUBLE_EQ(originalStream->GetPhysicalFormat().mSampleRate, 44100);
  EXPECT_EQ(originalStream->GetLatency(), 27u);
  EXPECT_DOUBLE_EQ(scenario.hardware.Value<Float64>(
                       scenario.DeviceID("speaker"),
                       Address(kAudioDevicePropertyNominalSampleRate)),
                   44100);

  const auto calls = scenario.hardware.Calls();
  std::optional<size_t> targetWrite;
  std::optional<size_t> streamRefresh;
  for (size_t index = callsBeforeChange; index < calls.size(); ++index) {
    if (calls[index].operation == FakeAudioHardware::Operation::Set &&
        calls[index].objectID == scenario.DeviceID("speaker") &&
        calls[index].address.mSelector ==
            kAudioDevicePropertyNominalSampleRate) {
      targetWrite = index;
    }
    if (!streamRefresh && calls[index].operation ==
                              FakeAudioHardware::Operation::GetSize &&
        calls[index].objectID == scenario.StreamID("speaker") &&
        calls[index].address.mSelector == kAudioStreamPropertyVirtualFormat) {
      streamRefresh = index;
    }
  }
  ASSERT_TRUE(targetWrite.has_value());
  ASSERT_TRUE(streamRefresh.has_value());
  EXPECT_LT(*targetWrite, *streamRefresh)
      << "the target rate must change before proxy stream refresh";
}

TEST(ProxyDeviceTest, RebuildsStreamsOnlyWhenIdentityOrTopologyChanges) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  const auto originalStream = proxy->GetStreamByIndex(aspl::Direction::Output, 0);

  constexpr AudioObjectID kReplacementStream = 900;
  scenario.hardware.Set(kReplacementStream, Address(kAudioObjectPropertyClass),
                        kAudioStreamClassID);
  scenario.hardware.Set<UInt32>(kReplacementStream,
                                Address(kAudioStreamPropertyDirection), 0);
  scenario.hardware.Set<UInt32>(kReplacementStream,
                                Address(kAudioStreamPropertyStartingChannel),
                                1);
  scenario.hardware.Set(kReplacementStream,
                        Address(kAudioStreamPropertyVirtualFormat),
                        Float32Format(44100, 2));
  scenario.hardware.Set<UInt32>(kReplacementStream,
                                Address(kAudioStreamPropertyLatency), 31);
  scenario.hardware.SetVector<AudioObjectID>(
      scenario.DeviceID("speaker"),
      Address(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput),
      {kReplacementStream});

  ASSERT_EQ(ProxyDeviceTestAccess::ApplyRate(*proxy, 44100), noErr);
  auto replacement = std::dynamic_pointer_cast<ProxyStream>(
      proxy->GetStreamByIndex(aspl::Direction::Output, 0));
  ASSERT_NE(replacement, nullptr);
  EXPECT_NE(replacement, originalStream);
  EXPECT_EQ(replacement->GetTargetObjectID(), kReplacementStream);
  EXPECT_EQ(replacement->GetLatency(), 31u);
}

TEST(ProxyDeviceTest, HandlesUnsupportedRatesRecursiveNotificationsAndRecovery) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);

  EXPECT_EQ(proxy->SetNominalSampleRateAsync(12345),
            kAudioHardwareUnsupportedOperationError);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 48000);

  scenario.hardware.Set(scenario.StreamID("speaker"),
                        Address(kAudioStreamPropertyVirtualFormat),
                        Float32Format(44100, 2));
  scenario.hardware.NotifyWhenSet(
      scenario.DeviceID("speaker"),
      Address(kAudioDevicePropertyNominalSampleRate));
  ASSERT_EQ(ProxyDeviceTestAccess::ApplyRate(*proxy, 44100), noErr);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 44100);

  scenario.hardware.Set(scenario.StreamID("speaker"),
                        Address(kAudioStreamPropertyVirtualFormat),
                        Float32Format(96000, 2));
  scenario.hardware.FailNext(
      FakeAudioHardware::Operation::Set, kAudioHardwareIllegalOperationError,
      scenario.DeviceID("speaker"),
      Address(kAudioDevicePropertyNominalSampleRate));
  EXPECT_EQ(ProxyDeviceTestAccess::ApplyRate(*proxy, 96000),
            kAudioHardwareIllegalOperationError);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 44100);

  EXPECT_EQ(ProxyDeviceTestAccess::ApplyRate(*proxy, 96000), noErr)
      << "the in-progress guard was not cleared after the failed update";
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 96000);
}

TEST(ProxyDeviceTest, FakeHostCompletesPublishedConfigurationRequests) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  auto plugin = std::make_shared<aspl::Plugin>(scenario.context);
  plugin->AddDevice(proxy, false);
  FakePluginHost host;
  scenario.context->Host = host.Ref();

  scenario.hardware.Set(scenario.StreamID("speaker"),
                        Address(kAudioStreamPropertyVirtualFormat),
                        Float32Format(44100, 2));
  ASSERT_EQ(proxy->SetNominalSampleRateAsync(44100), noErr);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 48000)
      << "a published device changed before host approval";
  ASSERT_EQ(host.ConfigurationRequests().size(), 1u);

  host.CompleteConfigurationRequests(plugin);
  EXPECT_DOUBLE_EQ(proxy->GetNominalSampleRate(), 44100);
  EXPECT_DOUBLE_EQ(scenario.hardware.Value<Float64>(
                       scenario.DeviceID("speaker"),
                       Address(kAudioDevicePropertyNominalSampleRate)),
                   44100);
  scenario.context->Host = nullptr;
}

}  // namespace
}  // namespace ProxyAudio::Test
