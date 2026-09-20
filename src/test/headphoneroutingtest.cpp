#include <gtest/gtest.h>

#include "soundio/headphonerouting.h"
#include "soundio/soundmanagerutil.h"

namespace {

SoundDeviceId deviceId(const QString& name) {
    SoundDeviceId id;
    id.name = name;
    return id;
}

AudioOutput output(AudioPath::AudioPathType type, unsigned char channelBase) {
    return AudioOutput(type, channelBase, mixxx::audio::ChannelCount::stereo());
}

int headphoneBase(const QMultiHash<SoundDeviceId, AudioOutput>& outputs,
        const SoundDeviceId& id) {
    const QList<AudioOutput> forDevice = outputs.values(id);
    for (const AudioOutput& out : forDevice) {
        if (out.getType() == AudioPath::AudioPathType::Headphones) {
            return out.getChannelGroup().getChannelBase();
        }
    }
    return -1;
}

TEST(HeadphoneRoutingTest, ControllerWithMainOnOneTwoGetsHeadphonesOnThreeFour) {
    const SoundDeviceId mixtrack = deviceId("Speakers (MixTrack Platinum FX)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(mixtrack, output(AudioPath::AudioPathType::Main, 0));
    QStringList considered;
    const auto result = HeadphoneRouting::apply(
            &outputs, {{mixtrack, QStringLiteral("MixTrack Platinum FX"), 4}}, &considered);
    EXPECT_TRUE(result.routed);
    EXPECT_EQ(QStringLiteral("MixTrack Platinum FX"), result.displayName);
    EXPECT_EQ(2, headphoneBase(outputs, mixtrack));
    EXPECT_EQ(QStringList{mixtrack.name}, considered);
}

TEST(HeadphoneRoutingTest, StereoDeviceIsLeftAlone) {
    const SoundDeviceId speakers = deviceId("Speakers (Realtek)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(speakers, output(AudioPath::AudioPathType::Main, 0));
    QStringList considered;
    EXPECT_FALSE(HeadphoneRouting::apply(&outputs, {{speakers, QString(), 2}}, &considered)
                    .routed);
    EXPECT_EQ(1, outputs.size());
    EXPECT_TRUE(considered.isEmpty());
}

TEST(HeadphoneRoutingTest, ExistingHeadphonesAreNeverMoved) {
    const SoundDeviceId mixtrack = deviceId("Speakers (MixTrack Platinum FX)");
    const SoundDeviceId laptop = deviceId("Headphones (Realtek)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(mixtrack, output(AudioPath::AudioPathType::Main, 0));
    outputs.insert(laptop, output(AudioPath::AudioPathType::Headphones, 0));
    QStringList considered;
    EXPECT_FALSE(HeadphoneRouting::apply(&outputs,
            {{mixtrack, QString(), 4}, {laptop, QString(), 2}},
            &considered)
                    .routed);
    EXPECT_EQ(-1, headphoneBase(outputs, mixtrack));
    // Still remembered, so removing the laptop headphones later does not
    // suddenly route them to the controller.
    EXPECT_EQ(QStringList{mixtrack.name}, considered);
}

TEST(HeadphoneRoutingTest, ADeviceIsOnlyEverRoutedOnce) {
    const SoundDeviceId mixtrack = deviceId("Speakers (MixTrack Platinum FX)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(mixtrack, output(AudioPath::AudioPathType::Main, 0));
    QStringList considered{mixtrack.name};
    EXPECT_FALSE(HeadphoneRouting::apply(&outputs, {{mixtrack, QString(), 4}}, &considered)
                    .routed);
    EXPECT_EQ(-1, headphoneBase(outputs, mixtrack));
}

TEST(HeadphoneRoutingTest, OccupiedThreeFourIsLeftAlone) {
    const SoundDeviceId card = deviceId("Speakers (Interface)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(card, output(AudioPath::AudioPathType::Main, 0));
    outputs.insert(card, output(AudioPath::AudioPathType::Booth, 2));
    QStringList considered;
    EXPECT_FALSE(HeadphoneRouting::apply(&outputs, {{card, QString(), 4}}, &considered)
                    .routed);
    EXPECT_EQ(-1, headphoneBase(outputs, card));
}

TEST(HeadphoneRoutingTest, MainOnAnotherPairIsNotAController) {
    const SoundDeviceId card = deviceId("Speakers (Interface)");
    QMultiHash<SoundDeviceId, AudioOutput> outputs;
    outputs.insert(card, output(AudioPath::AudioPathType::Main, 2));
    QStringList considered;
    EXPECT_FALSE(HeadphoneRouting::apply(&outputs, {{card, QString(), 4}}, &considered)
                    .routed);
    EXPECT_TRUE(considered.isEmpty());
}

} // namespace
