#include "mixer/synth.h"

#include <memory>

#include "control/controlobject.h"
#include "engine/channels/enginesynth.h"
#include "engine/enginemixer.h"
#include "moc_synth.cpp"

namespace {
/// Every note playable: no scale selected yet.
constexpr double kDefaultScaleMask = 4095.0;
/// C3, an octave either side of which is comfortable on a small keyboard.
constexpr double kDefaultBaseNote = 48.0;
} // namespace

Synth::Synth(PlayerManager* pParent,
        const QString& group,
        EngineMixer* pEngine,
        EffectsManager* pEffectsManager)
        : BasePlayer(pParent, group) {
    ChannelHandleAndGroup channelGroup = pEngine->registerChannelGroup(group);
    auto pSynth = std::make_unique<EngineSynth>(channelGroup, pEffectsManager);
    pEngine->addChannel(std::move(pSynth));

    // Shared note-entry state; see the class comment. Persisted, so the key
    // and octave you left the synth in are the ones you come back to.
    m_pScaleMask = std::make_unique<ControlObject>(ConfigKey(group, QStringLiteral("scale_mask")),
            true,
            false,
            /*bPersist*/ true,
            kDefaultScaleMask);
    m_pScaleRoot = std::make_unique<ControlObject>(ConfigKey(group, QStringLiteral("scale_root")),
            true,
            false,
            /*bPersist*/ true,
            0.0);
    m_pBaseNote = std::make_unique<ControlObject>(ConfigKey(group, QStringLiteral("base_note")),
            true,
            false,
            /*bPersist*/ true,
            kDefaultBaseNote);
}

Synth::~Synth() = default;
