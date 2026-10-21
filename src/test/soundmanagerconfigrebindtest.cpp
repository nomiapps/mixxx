#include <gtest/gtest.h>

#include "soundio/soundmanagerconfig.h"
#include "soundio/soundmanagerutil.h"

namespace {

SoundDeviceId device(const QString& name, int index, const QString& alsaHw = QString()) {
    SoundDeviceId id;
    id.name = name;
    id.deviceIndex = index;
    id.alsaHwDevice = alsaHw;
    return id;
}

const QString kMixtrack = QStringLiteral("Speakers (2- MixTrack Platinum FX)");
const QString kLaptop = QStringLiteral("Speakers (Realtek High Definition Audio(SST))");

TEST(SoundManagerConfigRebindTest, ReplugTakesTheNewIndex) {
    // The Surface on 22 Oct 2026: the Mixtrack was index 8 at startup and
    // came back elsewhere after a replug.
    const SoundDeviceId configured = device(kMixtrack, 8);
    const QList<SoundDeviceId> available = {device(kLaptop, 10), device(kMixtrack, 13)};
    EXPECT_EQ(device(kMixtrack, 13),
            SoundManagerConfig::reboundDeviceId(configured, available));
}

TEST(SoundManagerConfigRebindTest, MissingDeviceIsKeptSoItReportsNotFound) {
    const SoundDeviceId configured = device(kMixtrack, 8);
    const QList<SoundDeviceId> available = {device(kLaptop, 10)};
    EXPECT_EQ(configured, SoundManagerConfig::reboundDeviceId(configured, available));
}

TEST(SoundManagerConfigRebindTest, SameNameDecidedByAlsaHwName) {
    const QString name = QStringLiteral("USB Audio");
    const SoundDeviceId configured = device(name, 3, QStringLiteral("hw:2,0"));
    const QList<SoundDeviceId> available = {
            device(name, 4, QStringLiteral("hw:1,0")),
            device(name, 5, QStringLiteral("hw:2,0"))};
    EXPECT_EQ(available[1], SoundManagerConfig::reboundDeviceId(configured, available));
}

TEST(SoundManagerConfigRebindTest, SameNameKeepsTheOldIndexWhenStillThere) {
    const QString name = QStringLiteral("USB Audio");
    const SoundDeviceId configured = device(name, 5);
    const QList<SoundDeviceId> available = {device(name, 4), device(name, 5)};
    EXPECT_EQ(available[1], SoundManagerConfig::reboundDeviceId(configured, available));
}

TEST(SoundManagerConfigRebindTest, RebindMovesEveryPathOfTheDevice) {
    SoundManagerConfig config(nullptr);
    const SoundDeviceId before = device(kMixtrack, 8);
    const AudioOutput main(AudioPathType::Main, 0, mixxx::audio::ChannelCount::stereo());
    const AudioOutput headphones(
            AudioPathType::Headphones, 2, mixxx::audio::ChannelCount::stereo());
    config.addOutput(before, main);
    config.addOutput(before, headphones);

    const SoundDeviceId after = device(kMixtrack, 13);
    config.rebindDevices({device(kLaptop, 10), after});

    const auto outputs = config.getOutputs();
    EXPECT_FALSE(outputs.contains(before));
    const QList<AudioOutput> moved = outputs.values(after);
    EXPECT_EQ(2, moved.size());
    EXPECT_TRUE(moved.contains(main));
    EXPECT_TRUE(moved.contains(headphones));
}

} // namespace
