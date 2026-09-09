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

/// Eight pattern slots for the step sequencer, kept as JSON in the settings
/// directory (sequencer-patterns.json). The LIVE pattern is the [SequencerN]
/// controls themselves, which persist on their own; a slot is a snapshot of
/// them. Selecting a filled slot applies it; selecting an empty one leaves
/// the live pattern alone, so "select an empty slot, SAVE" is how a pattern
/// gets copied.
///
/// Controls, in the sequencer's group:
///   pattern            1..8, the current slot (persists)
///   pattern_save       snapshot the live pattern into the current slot
///   pattern_load       re-apply the current slot, discarding edits
///   pattern_N_filled   read-only, 1 while slot N holds a pattern
///
/// Main thread only. It reads and writes controls the engine only ever reads,
/// and its slots do file I/O -- so the control connections are the default
/// AutoConnection, never Direct: a change made from the controller or the
/// engine thread is queued here. Applying a slot sets ~130 controls one at a
/// time while the engine keeps reading them; at most one step can see a mix
/// of old and new values, which is accepted.
class SequencerPatternBank : public QObject {
    Q_OBJECT
  public:
    static constexpr int kSlots = 8;

    /// filePathOverride replaces <settingsPath>/sequencer-patterns.json (tests).
    SequencerPatternBank(const QString& group,
            UserSettingsPointer pConfig,
            const QString& filePathOverride = QString());
    ~SequencerPatternBank() override;

    /// Slots are 1-based, like the pattern control.
    bool saveSlot(int slot);
    /// False when the slot is empty; the live pattern is then untouched.
    bool loadSlot(int slot);
    bool isFilled(int slot) const;
    int currentSlot() const {
        return m_currentSlot;
    }
    QString filePath() const {
        return m_filePath;
    }

  private slots:
    void slotPatternChanged(double v);
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
    ControlObject* m_pPattern;
    ControlPushButton* m_pSave;
    ControlPushButton* m_pLoad;
    std::array<ControlObject*, kSlots> m_filled;
    // Every control that is part of a pattern. Never run, restart or
    // current_step: a saved pattern must not start or stop anything.
    QVector<ConfigKey> m_liveKeys;
    std::array<QJsonObject, kSlots> m_slots; // isEmpty() == empty slot
    int m_currentSlot;
};
