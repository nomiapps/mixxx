#include "soundio/headphonerouting.h"

HeadphoneRouting::Result HeadphoneRouting::apply(
        QMultiHash<SoundDeviceId, AudioOutput>* pOutputs,
        const QList<Device>& devices,
        QStringList* pConsidered) {
    Result result;
    bool headphonesAssigned = false;
    for (const AudioOutput& output : std::as_const(*pOutputs)) {
        if (output.getType() == AudioPath::AudioPathType::Headphones) {
            headphonesAssigned = true;
            break;
        }
    }
    const AudioOutput headphones(AudioPath::AudioPathType::Headphones,
            2,
            mixxx::audio::ChannelCount::stereo());
    for (const Device& device : devices) {
        if (device.outputChannels < 4) {
            continue;
        }
        const QList<AudioOutput> onDevice = pOutputs->values(device.id);
        bool mainOnFirstPair = false;
        for (const AudioOutput& output : onDevice) {
            if (output.getType() == AudioPath::AudioPathType::Main &&
                    output.getChannelGroup().getChannelBase() == 0) {
                mainOnFirstPair = true;
                break;
            }
        }
        if (!mainOnFirstPair || pConsidered->contains(device.id.name)) {
            continue;
        }
        pConsidered->append(device.id.name);
        if (headphonesAssigned || result.routed) {
            continue;
        }
        bool clash = false;
        for (const AudioOutput& output : onDevice) {
            if (output.channelsClash(headphones)) {
                clash = true;
                break;
            }
        }
        if (clash) {
            continue;
        }
        pOutputs->insert(device.id, headphones);
        result.routed = true;
        result.device = device.id;
        result.displayName = device.displayName;
    }
    return result;
}
