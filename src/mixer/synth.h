#pragma once

#include <QObject>
#include <QString>

#include "mixer/baseplayer.h"

class EffectsManager;
class EngineMixer;

/// A Synth is the player-side owner of an EngineSynth channel: a sound
/// source with no track and no soundcard input, so unlike Deck or Auxiliary
/// there is nothing to load or configure. It exists so PlayerManager can
/// create, count and address synths ("[Synth1]") the same way it does the
/// other channel kinds.
class Synth : public BasePlayer {
    Q_OBJECT
  public:
    Synth(PlayerManager* pParent,
            const QString& group,
            EngineMixer* pMixingEngine,
            EffectsManager* pEffectsManager);
    ~Synth() override = default;
};
