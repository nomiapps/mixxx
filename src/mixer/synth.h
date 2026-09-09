#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "mixer/baseplayer.h"

class ControlObject;
class EffectsManager;
class EngineMixer;

/// A Synth is the player-side owner of an EngineSynth channel: a sound
/// source with no track and no soundcard input, so unlike Deck or Auxiliary
/// there is nothing to load or configure. It exists so PlayerManager can
/// create, count and address synths ("[Synth1]") the same way it does the
/// other channel kinds.
///
/// It also owns the note-entry state that every surface playing this synth
/// shares. None of it reaches the audio path -- the engine is told about notes
/// and nothing else -- so it lives here rather than on EngineSynth. Its point
/// is that a keyboard on screen and a pad controller in front of it agree on
/// what key they are in and which octave is under the hand, and that changing
/// it on one moves the other. In the synth's group:
///
///   scale_mask  the scale as twelve bits, bit N set when the note N semitones
///               above the root is in it. Bit 0, the root, is always set. The
///               default 4095 is every note: chromatic, nothing filtered.
///   scale_root  the pitch class the mask is rooted on, 0..11, 0 = C.
///   base_note   the MIDI note at the bottom of the keyboard.
///
/// A mask rather than an index into a table of scales, so that two surfaces
/// cannot disagree about which notes are in one: reading it needs no shared
/// table, only the names do. All three persist, so a set-up survives a restart.
class Synth : public BasePlayer {
    Q_OBJECT
  public:
    Synth(PlayerManager* pParent,
            const QString& group,
            EngineMixer* pMixingEngine,
            EffectsManager* pEffectsManager);
    ~Synth() override;

  private:
    std::unique_ptr<ControlObject> m_pScaleMask;
    std::unique_ptr<ControlObject> m_pScaleRoot;
    std::unique_ptr<ControlObject> m_pBaseNote;
};
