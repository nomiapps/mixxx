#include "mixer/synth.h"

#include <memory>

#include "engine/channels/enginesynth.h"
#include "engine/enginemixer.h"
#include "moc_synth.cpp"

Synth::Synth(PlayerManager* pParent,
        const QString& group,
        EngineMixer* pEngine,
        EffectsManager* pEffectsManager)
        : BasePlayer(pParent, group) {
    ChannelHandleAndGroup channelGroup = pEngine->registerChannelGroup(group);
    auto pSynth = std::make_unique<EngineSynth>(channelGroup, pEffectsManager);
    pEngine->addChannel(std::move(pSynth));
}
