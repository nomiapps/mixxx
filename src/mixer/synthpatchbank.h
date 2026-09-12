#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <array>

#include "preferences/configobject.h"
#include "preferences/usersettings.h"

class ControlObject;
class ControlPushButton;

/// Eight patch slots for a synth, kept as JSON in the settings directory
/// (synth-patches.json). A patch is a snapshot of the synth's timbre
/// controls: oscillators, wavetable and position, envelope, filter, gain,
/// LFO and unison. It is NOT what is being played: note events, the key,
/// scale and octave the keyboard sits in (a patch recalled mid-set must not
/// move the keys under the hand), the channel's routing and fader, or the
/// read-only frame count and LFO phase.
///
/// Controls, in the synth's group:
///   patch            1..8, the current slot (persists)
///   patch_save       snapshot the live sound into the current slot
///   patch_load       re-apply the current slot, discarding edits
///   patch_N_filled   read-only, 1 while slot N holds a patch
///
/// Selecting a filled slot applies it; selecting an empty one leaves the
/// live sound alone, so "select an empty slot, SAVE" copies a patch.
///
/// Unlike the sequencer's pattern bank, whose live controls persist on
/// their own, the synth's timbre controls do not: so on construction the
/// persisted current slot is applied when it is filled, and the synth comes
/// back sounding as it was left rather than reading "3" while playing
/// defaults.
///
/// Main thread only. Its slots do file I/O, so the control connections are
/// the default AutoConnection, never Direct: a change from a controller or
/// the engine thread is queued here. Applying a slot sets two dozen controls
/// one at a time while the engine keeps reading them once per buffer; one
/// buffer can see a mix of old and new values, which is accepted.
class SynthPatchBank : public QObject {
    Q_OBJECT
  public:
    static constexpr int kSlots = 8;

    /// filePathOverride replaces <settingsPath>/synth-patches.json (tests).
    SynthPatchBank(const QString& group,
            UserSettingsPointer pConfig,
            const QString& filePathOverride = QString());
    ~SynthPatchBank() override;

    /// Slots are 1-based, like the patch control.
    bool saveSlot(int slot);
    /// False when the slot is empty; the live sound is then untouched.
    bool loadSlot(int slot);
    bool isFilled(int slot) const;
    int currentSlot() const {
        return m_currentSlot;
    }
    QString filePath() const {
        return m_filePath;
    }
    /// Every control a patch carries, in the synth's group (tests).
    const QVector<ConfigKey>& liveKeys() const {
        return m_liveKeys;
    }

  private slots:
    void slotPatchChanged(double v);
    void slotSave(double v);
    void slotLoad(double v);

  private:
    QJsonObject captureLive() const;
    void applyToLive(const QJsonObject& slot);
    void readFile();
    bool writeFile() const;
    void publishFilled();

    const QString m_group;
    const QString m_filePath;
    ControlObject* m_pPatch;
    ControlPushButton* m_pSave;
    ControlPushButton* m_pLoad;
    std::array<ControlObject*, kSlots> m_filled;
    QVector<ConfigKey> m_liveKeys;
    std::array<QJsonObject, kSlots> m_slots; // isEmpty() == empty slot
    int m_currentSlot;
};
