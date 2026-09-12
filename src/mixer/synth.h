#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <optional>

#include "engine/channels/wavetable.h"
#include "mixer/baseplayer.h"
#include "preferences/usersettings.h"

class ControlObject;
class EffectsManager;
class EngineMixer;
class WavetableLibrary;

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
///
/// The wavetable the engine's wave 4 plays is chosen here too, for the same
/// reason: which table is an index into a list only the main thread can scan
/// (see WavetableLibrary), and only the samples cross to the engine, down the
/// lane described in wavetable.h. Two copies of the chosen table exist, one
/// for the engine and one (currentTable) for the display, so neither side
/// ever waits for the other.
///
///   wavetable   index into wavetableNames(); persists. Selecting a table
///               loads it; a file that fails to load is reported and the
///               engine keeps playing the previous one.
///   wt_frames   read-only: frames in the loaded table, 0 while none is.
class Synth : public BasePlayer {
    Q_OBJECT
  public:
    Synth(PlayerManager* pParent,
            const QString& group,
            UserSettingsPointer pConfig,
            EngineMixer* pMixingEngine,
            EffectsManager* pEffectsManager);
    ~Synth() override;

    QStringList wavetableNames() const;
    int wavetableCount() const;
    /// The selected index, always within the list.
    int currentWavetable() const;
    /// Selects (clamped) and loads; reloads when already selected.
    void selectWavetable(int index);
    /// The display's copy of the loaded table; nullptr while none is loaded.
    /// Main thread only. Immutable, so it can be drawn from without a lock.
    std::shared_ptr<const Wavetable> currentTable() const {
        return m_pUiTable;
    }
    /// Re-reads the wavetable folder. The selection is kept by index.
    void rescanWavetables();

  signals:
    void wavetableNamesChanged();
    /// currentTable() changed: a table loaded, or a load failed and it is
    /// now empty.
    void wavetableChanged();

  private slots:
    void slotWavetableControlChanged(double value);
    void slotWavetableLoaded(int index, std::shared_ptr<const Wavetable> pTable);
    void slotWavetableLoadFailed(int index, const QString& reason);

  private:
    void sendToEngine(const Wavetable& table);
    void collectRetiredTables();

    std::unique_ptr<ControlObject> m_pScaleMask;
    std::unique_ptr<ControlObject> m_pScaleRoot;
    std::unique_ptr<ControlObject> m_pBaseNote;

    std::optional<WavetableMainPipe> m_wavetablePipe;
    std::unique_ptr<WavetableLibrary> m_pLibrary;
    std::shared_ptr<const Wavetable> m_pUiTable;
    std::unique_ptr<ControlObject> m_pWavetable;
    std::unique_ptr<ControlObject> m_pWtFrames;
};
