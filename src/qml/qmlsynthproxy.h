#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <memory>

#include "mixer/playermanager.h"

class Synth;

namespace mixxx {
namespace qml {

/// The synths' non-audio state for QML, by group: today the wavetable list
/// and selection (see Synth). A singleton like PlayerManager, because a panel
/// only knows its group string and the synth behind it lives in
/// PlayerManager.
class QmlSynthProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Synth)
    QML_SINGLETON

  public:
    explicit QmlSynthProxy(std::shared_ptr<PlayerManager> pPlayerManager,
            QObject* parent = nullptr);

    Q_INVOKABLE QStringList wavetableNames(const QString& group) const;
    /// The selected table's name, empty when the group is not a synth.
    Q_INVOKABLE QString wavetableName(const QString& group) const;
    Q_INVOKABLE int wavetableCount(const QString& group) const;
    /// Frames in the loaded table; 0 while none is loaded.
    Q_INVOKABLE int wavetableFrames(const QString& group) const;
    Q_INVOKABLE void selectWavetable(const QString& group, int index);
    /// Moves the selection by delta, wrapping at either end.
    Q_INVOKABLE void stepWavetable(const QString& group, int delta);
    Q_INVOKABLE void rescanWavetables(const QString& group);
    /// The engine's LFO shape function, for drawing it: -1..1 for shape
    /// 0..4 at a phase whose integer part is the cycle.
    Q_INVOKABLE double lfoValue(int shape, double phase) const;
    /// The envelope knob's seconds for stage 0 attack, 1 decay, 2 release.
    Q_INVOKABLE double envelopeSeconds(double param, int stage) const;
    Q_INVOKABLE double cutoffHz(double param) const;
    /// The low-pass filter's gain in dB at hz for the two knob settings.
    Q_INVOKABLE double filterResponseDb(double cutoffParam, double resonanceParam, double hz) const;
    /// The shipped patches: how many, the name of one, and applying one to
    /// the live sound (the user's slots are untouched).
    Q_INVOKABLE int factoryPatchCount(const QString& group) const;
    Q_INVOKABLE QString factoryPatchName(const QString& group, int index) const;
    Q_INVOKABLE void applyFactoryPatch(const QString& group, int index);

    /// The Synth behind a "[SynthN]" group, nullptr for anything else. For
    /// the QML items that watch one synth directly.
    static Synth* synthForGroup(const QString& group);

    static QmlSynthProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static void registerPlayerManager(std::shared_ptr<PlayerManager> pPlayerManager) {
        s_pPlayerManager = std::move(pPlayerManager);
    }

  signals:
    /// The table shown for group changed: loaded, or failed and now empty.
    void wavetableChanged(const QString& group);
    void wavetableNamesChanged(const QString& group);

  private:
    static inline std::shared_ptr<PlayerManager> s_pPlayerManager;

    const std::shared_ptr<PlayerManager> m_pPlayerManager;
};

} // namespace qml
} // namespace mixxx
