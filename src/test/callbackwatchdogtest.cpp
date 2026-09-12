#include <gtest/gtest.h>

#include "soundio/callbackwatchdog.h"
#include "soundio/soundmanagerutil.h"

namespace {

SoundDeviceId device(const QString& name) {
    SoundDeviceId id;
    id.name = name;
    return id;
}

QList<CallbackWatchdog::Sample> one(const SoundDeviceId& id, uint64_t count) {
    return {{id, count}};
}

TEST(CallbackWatchdogTest, AdvancingDeviceIsNeverStalled) {
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    EXPECT_TRUE(watchdog.tick(one(speakers, 100)).stalled.isEmpty());
    for (uint64_t count = 101; count < 120; ++count) {
        const auto result = watchdog.tick(one(speakers, count));
        EXPECT_TRUE(result.stalled.isEmpty());
        EXPECT_TRUE(result.anyAdvanced);
    }
}

TEST(CallbackWatchdogTest, QuietDeviceIsReportedAfterThreeTicksAndThenOnce) {
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    watchdog.tick(one(speakers, 500));
    // The counter freezes: two quiet ticks are tolerated, the third reports.
    EXPECT_TRUE(watchdog.tick(one(speakers, 500)).stalled.isEmpty());
    EXPECT_TRUE(watchdog.tick(one(speakers, 500)).stalled.isEmpty());
    const auto stalled = watchdog.tick(one(speakers, 500));
    ASSERT_EQ(stalled.stalled.size(), 1);
    EXPECT_EQ(stalled.stalled.first().name, QStringLiteral("Speakers"));
    EXPECT_FALSE(stalled.anyAdvanced);
    // Reported once; the count starts over, so the next report is three
    // ticks away again rather than every tick.
    EXPECT_TRUE(watchdog.tick(one(speakers, 500)).stalled.isEmpty());
    EXPECT_TRUE(watchdog.tick(one(speakers, 500)).stalled.isEmpty());
    EXPECT_EQ(watchdog.tick(one(speakers, 500)).stalled.size(), 1);
}

TEST(CallbackWatchdogTest, DeviceThatNeverCallsBackIsStalledToo) {
    // An open stream whose counter sits at zero for three ticks after the
    // first sight is as dead as one that stopped.
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    watchdog.tick(one(speakers, 0));
    watchdog.tick(one(speakers, 0));
    watchdog.tick(one(speakers, 0));
    EXPECT_EQ(watchdog.tick(one(speakers, 0)).stalled.size(), 1);
}

TEST(CallbackWatchdogTest, OnlyTheQuietDeviceOfTwoIsReported) {
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    const SoundDeviceId headphones = device("Headphones");
    uint64_t live = 10;
    watchdog.tick({{speakers, live}, {headphones, 7}});
    CallbackWatchdog::Result result;
    for (int i = 0; i < CallbackWatchdog::kStalledTicks; ++i) {
        result = watchdog.tick({{speakers, ++live}, {headphones, 7}});
    }
    ASSERT_EQ(result.stalled.size(), 1);
    EXPECT_EQ(result.stalled.first().name, QStringLiteral("Headphones"));
    EXPECT_TRUE(result.anyAdvanced);
}

TEST(CallbackWatchdogTest, ReopenedDeviceStartsClean) {
    // Two quiet ticks, then the device is closed (absent from a tick) and
    // reopened with a fresh counter: the quiet run does not carry over.
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    watchdog.tick(one(speakers, 40));
    watchdog.tick(one(speakers, 40));
    watchdog.tick(one(speakers, 40));
    watchdog.tick({});
    watchdog.tick(one(speakers, 3));
    EXPECT_TRUE(watchdog.tick(one(speakers, 3)).stalled.isEmpty());
    EXPECT_TRUE(watchdog.tick(one(speakers, 3)).stalled.isEmpty());
    EXPECT_EQ(watchdog.tick(one(speakers, 3)).stalled.size(), 1);
}

TEST(CallbackWatchdogTest, ResetForgetsEverything) {
    CallbackWatchdog watchdog;
    const SoundDeviceId speakers = device("Speakers");
    watchdog.tick(one(speakers, 40));
    watchdog.tick(one(speakers, 40));
    watchdog.tick(one(speakers, 40));
    watchdog.reset();
    EXPECT_TRUE(watchdog.tick(one(speakers, 40)).stalled.isEmpty());
    EXPECT_TRUE(watchdog.tick(one(speakers, 40)).stalled.isEmpty());
    EXPECT_TRUE(watchdog.tick(one(speakers, 40)).stalled.isEmpty());
    EXPECT_EQ(watchdog.tick(one(speakers, 40)).stalled.size(), 1);
}

} // namespace
