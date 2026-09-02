// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <vector>

#include "TestSupport.hpp"

namespace ProxyAudio::Test {
namespace {

TEST(ProxyIOTest, RejectsMissingZeroChannelAndNonFloatOutputStreams) {
  {
    ProxyAudioScenario scenario;
    DeviceSpec spec;
    spec.hasOutputStream = false;
    spec.hasVolume = false;
    spec.hasMute = false;
    scenario.AddDevice("no-output", spec);
    auto proxy = scenario.CreateProxy("no-output");
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->OnStartIO(), kAudioHardwareNotRunningError);
    EXPECT_EQ(scenario.hardware.CallCount(
                  FakeAudioHardware::Operation::CreateIOProc),
              0u);
  }

  {
    ProxyAudioScenario scenario;
    DeviceSpec spec;
    spec.channels = 0;
    scenario.AddDevice("zero-channel", spec);
    auto proxy = scenario.CreateProxy("zero-channel");
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->OnStartIO(), kAudioHardwareNotRunningError);
  }

  {
    ProxyAudioScenario scenario;
    scenario.AddDevice("integer");
    auto format = Float32Format();
    format.mFormatFlags = kAudioFormatFlagIsSignedInteger |
                          kAudioFormatFlagIsPacked;
    scenario.hardware.Set(scenario.StreamID("integer"),
                          Address(kAudioStreamPropertyVirtualFormat), format);
    auto proxy = scenario.CreateProxy("integer");
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->OnStartIO(), kAudioHardwareNotRunningError);
  }
}

TEST(ProxyIOTest, HandlesCreateAndAsynchronousStartFailures) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);

  scenario.hardware.FailNext(FakeAudioHardware::Operation::CreateIOProc,
                             kAudioHardwareUnspecifiedError,
                             scenario.DeviceID("speaker"));
  EXPECT_EQ(proxy->OnStartIO(), kAudioHardwareUnspecifiedError);
  EXPECT_FALSE(ProxyDeviceTestAccess::IsRunning(*proxy));
  EXPECT_EQ(ProxyDeviceTestAccess::RingCapacity(*proxy), 0u);

  scenario.hardware.QueueStartResult(kAudioHardwareNotRunningError);
  ASSERT_EQ(proxy->OnStartIO(), noErr);
  EXPECT_TRUE(ProxyDeviceTestAccess::IsRunning(*proxy));
  ASSERT_EQ(scenario.dispatch.Pending(), 1u);
  scenario.dispatch.DrainOne();
  EXPECT_FALSE(ProxyDeviceTestAccess::IsRunning(*proxy));
  proxy->OnStopIO();

  EXPECT_EQ(scenario.hardware.CallCount(
                FakeAudioHardware::Operation::CreateIOProc),
            2u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StartIOProc),
      1u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StopIOProc),
      1u);
  EXPECT_EQ(scenario.hardware.CallCount(
                FakeAudioHardware::Operation::DestroyIOProc),
            1u);
}

TEST(ProxyIOTest, StopBeforeStartAndStaleFailureCannotCorruptNewSession) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);

  scenario.hardware.QueueStartResult(kAudioHardwareNotRunningError);
  scenario.hardware.QueueStartResult(noErr);
  ASSERT_EQ(proxy->OnStartIO(), noErr);
  const auto firstGeneration = ProxyDeviceTestAccess::Generation(*proxy);
  proxy->OnStopIO();
  ASSERT_EQ(proxy->OnStartIO(), noErr);
  EXPECT_GT(ProxyDeviceTestAccess::Generation(*proxy), firstGeneration);
  EXPECT_TRUE(ProxyDeviceTestAccess::IsRunning(*proxy));

  scenario.dispatch.DrainOne();
  EXPECT_TRUE(ProxyDeviceTestAccess::IsRunning(*proxy))
      << "a failure from the destroyed session changed the new session";
  scenario.dispatch.DrainOne();
  EXPECT_TRUE(ProxyDeviceTestAccess::IsRunning(*proxy));
  proxy->OnStopIO();

  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::CreateIOProc),
      2u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StartIOProc),
      2u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StopIOProc),
      2u);
  EXPECT_EQ(scenario.hardware.CallCount(
                FakeAudioHardware::Operation::DestroyIOProc),
            2u);
}

TEST(ProxyIOTest, PreservesSamplesAcrossBuffersAndZeroFillsUnderruns) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  scenario.Start("speaker");
  scenario.dispatch.DrainAll();

  const std::vector<Float32> samples{0.1f, 0.2f, 0.3f, 0.4f,
                                     0.5f, 0.6f, 0.7f, 0.8f};
  scenario.Write("speaker", samples);
  auto output = scenario.ReadTarget("speaker", {3, 5});
  ASSERT_EQ(output.status, noErr);
  EXPECT_EQ(output.buffers[0],
            (std::vector<Float32>{0.1f, 0.2f, 0.3f}));
  EXPECT_EQ(output.buffers[1],
            (std::vector<Float32>{0.4f, 0.5f, 0.6f, 0.7f, 0.8f}));
  EXPECT_EQ(ProxyDeviceTestAccess::Underruns(*proxy), 0u);

  scenario.Write("speaker", {0.25f, -0.25f});
  output = scenario.ReadTarget("speaker", {6}, 1.0f);
  EXPECT_EQ(output.buffers[0],
            (std::vector<Float32>{0.25f, -0.25f, 0.0f, 0.0f, 0.0f,
                                  0.0f}));
  EXPECT_EQ(ProxyDeviceTestAccess::Underruns(*proxy), 1u);
  scenario.Stop("speaker");
}

TEST(ProxyIOTest, AccountsForOverrunsAndPeriodicCounters) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  scenario.Start("speaker");

  const auto capacity = ProxyDeviceTestAccess::RingCapacity(*proxy);
  ASSERT_EQ(capacity, 8192u * 2u);
  std::vector<Float32> samples(capacity + 7, 0.5f);
  scenario.Write("speaker", samples);
  EXPECT_EQ(ProxyDeviceTestAccess::RingCount(*proxy), capacity);
  EXPECT_EQ(ProxyDeviceTestAccess::OverrunSamples(*proxy), 7u);
  EXPECT_EQ(ProxyDeviceTestAccess::WriteCalls(*proxy), 1u);
  EXPECT_EQ(ProxyDeviceTestAccess::WriteSamples(*proxy), capacity + 7);
  EXPECT_EQ(ProxyDeviceTestAccess::MaxWriteChunk(*proxy), capacity + 7);

  for (size_t call = 0; call < 500; ++call) {
    ASSERT_EQ(scenario.ReadTarget("speaker", {0}).status, noErr);
  }
  EXPECT_EQ(ProxyDeviceTestAccess::IOProcCalls(*proxy), 500u);
  EXPECT_EQ(ProxyDeviceTestAccess::RingFillMinimum(*proxy), SIZE_MAX);
  EXPECT_EQ(ProxyDeviceTestAccess::RingFillMaximum(*proxy), 0u);
  scenario.Stop("speaker");
}

TEST(ProxyIOTest, AnchorsClockAdvancesPeriodsAndResetsEverySession) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);

  SetFakeHostTime(100);
  scenario.Start("speaker");
  Float64 sampleTime = -1;
  UInt64 hostTime = 0;
  UInt64 seed = 0;
  ASSERT_EQ(proxy->GetZeroTimeStampImpl(0, &sampleTime, &hostTime, &seed),
            noErr);
  EXPECT_DOUBLE_EQ(sampleTime, 0);
  EXPECT_EQ(hostTime, 100u);
  EXPECT_EQ(seed, 1u);

  SetFakeHostTime(200);
  ASSERT_EQ(scenario.ReadTarget("speaker", {2}).status, noErr);
  ASSERT_EQ(proxy->GetZeroTimeStampImpl(0, &sampleTime, &hostTime, &seed),
            noErr);
  EXPECT_DOUBLE_EQ(sampleTime, 0);
  EXPECT_EQ(hostTime, 200u);

  SetFakeHostTime(300);
  ASSERT_EQ(scenario.ReadTarget("speaker", {32766}).status, noErr);
  ASSERT_EQ(proxy->GetZeroTimeStampImpl(0, &sampleTime, &hostTime, &seed),
            noErr);
  EXPECT_DOUBLE_EQ(sampleTime, 16384);
  EXPECT_EQ(hostTime, 300u);
  EXPECT_EQ(ProxyDeviceTestAccess::FramesRead(*proxy), 16384u);

  scenario.Stop("speaker");
  EXPECT_EQ(ProxyDeviceTestAccess::FramesRead(*proxy), 0u);
  EXPECT_EQ(ProxyDeviceTestAccess::Underruns(*proxy), 0u);
  EXPECT_EQ(ProxyDeviceTestAccess::IOProcCalls(*proxy), 0u);
  EXPECT_EQ(ProxyDeviceTestAccess::OverrunSamples(*proxy), 0u);
  EXPECT_EQ(ProxyDeviceTestAccess::Timestamp(*proxy).readTimestamp_, 0u);

  SetFakeHostTime(400);
  scenario.Start("speaker");
  EXPECT_EQ(ProxyDeviceTestAccess::WriteCalls(*proxy), 0u);
  EXPECT_EQ(ProxyDeviceTestAccess::RingCount(*proxy), 0u);
  ASSERT_EQ(proxy->GetZeroTimeStampImpl(0, &sampleTime, &hostTime, &seed),
            noErr);
  EXPECT_EQ(hostTime, 400u);
  scenario.Stop("speaker");
  scenario.dispatch.DrainAll();
}

TEST(ProxyIOTest, LateCallbacksAfterShutdownAreSilentAndCleanupIsBalanced) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = scenario.CreateProxy("speaker");
  ASSERT_NE(proxy, nullptr);
  scenario.Start("speaker");
  scenario.dispatch.DrainAll();
  scenario.Write("speaker", {0.5f, -0.5f, 0.25f, -0.25f});
  scenario.Stop("speaker");

  const auto late = scenario.hardware.InvokeLastDestroyedIOProc(
      scenario.DeviceID("speaker"), {4, 2}, 1.0f);
  EXPECT_EQ(late.status, noErr);
  EXPECT_EQ(late.buffers[0], (std::vector<Float32>(4, 0.0f)));
  EXPECT_EQ(late.buffers[1], (std::vector<Float32>(2, 0.0f)));
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::CreateIOProc),
      1u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StartIOProc),
      1u);
  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StopIOProc),
      1u);
  EXPECT_EQ(scenario.hardware.CallCount(
                FakeAudioHardware::Operation::DestroyIOProc),
            1u);
}

TEST(ProxyIOTest, DestructorStopsAndDestroysAnOutstandingIOProc) {
  ProxyAudioScenario scenario;
  scenario.AddDevice("speaker");
  auto proxy = ProxyDevice::Create(scenario.DeviceID("speaker"),
                                   scenario.context);
  ASSERT_NE(proxy, nullptr);
  ASSERT_EQ(proxy->OnStartIO(), noErr);
  scenario.dispatch.DrainAll();
  proxy.reset();

  EXPECT_EQ(
      scenario.hardware.CallCount(FakeAudioHardware::Operation::StopIOProc),
      1u);
  EXPECT_EQ(scenario.hardware.CallCount(
                FakeAudioHardware::Operation::DestroyIOProc),
            1u);
}

}  // namespace
}  // namespace ProxyAudio::Test
