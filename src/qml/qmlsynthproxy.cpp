#include "qml/qmlsynthproxy.h"

#include <QtDebug>
#include <cmath>

#include "control/controlobject.h"
#include "engine/channels/enginesynth.h"
#include "mixer/synth.h"
#include "mixer/synthpatchbank.h"
#include "moc_qmlsynthproxy.cpp"
#include "util/assert.h"

namespace mixxx {
namespace qml {

namespace {
const QString kAppGroup = QStringLiteral("[App]");
} // namespace

QmlSynthProxy::QmlSynthProxy(std::shared_ptr<PlayerManager> pPlayerManager, QObject* parent)
        : QObject(parent),
          m_pPlayerManager(std::move(pPlayerManager)) {
    // Relay every synth's signals with its group attached, so a panel can
    // watch this singleton rather than hold a synth pointer of its own.
    const int count = static_cast<int>(std::lround(
            ControlObject::get(ConfigKey(kAppGroup, QStringLiteral("num_synths")))));
    for (int i = 1; i <= count; ++i) {
        Synth* pSynth = m_pPlayerManager->getSynth(i);
        if (pSynth == nullptr) {
            continue;
        }
        const QString group = pSynth->getGroup();
        connect(pSynth, &Synth::wavetableChanged, this, [this, group] {
            emit wavetableChanged(group);
        });
        connect(pSynth, &Synth::wavetableNamesChanged, this, [this, group] {
            emit wavetableNamesChanged(group);
        });
    }
}

// static
Synth* QmlSynthProxy::synthForGroup(const QString& group) {
    if (!s_pPlayerManager) {
        return nullptr;
    }
    int number = 0;
    if (!PlayerManager::isSynthGroup(group, &number) || number < 1) {
        return nullptr;
    }
    return s_pPlayerManager->getSynth(static_cast<unsigned int>(number));
}

QStringList QmlSynthProxy::wavetableNames(const QString& group) const {
    Synth* pSynth = synthForGroup(group);
    return pSynth ? pSynth->wavetableNames() : QStringList();
}

QString QmlSynthProxy::wavetableName(const QString& group) const {
    Synth* pSynth = synthForGroup(group);
    if (pSynth == nullptr || pSynth->wavetableCount() <= 0) {
        return QString();
    }
    return pSynth->wavetableNames().value(pSynth->currentWavetable());
}

int QmlSynthProxy::wavetableCount(const QString& group) const {
    Synth* pSynth = synthForGroup(group);
    return pSynth ? pSynth->wavetableCount() : 0;
}

int QmlSynthProxy::wavetableFrames(const QString& group) const {
    Synth* pSynth = synthForGroup(group);
    const auto pTable = pSynth ? pSynth->currentTable() : nullptr;
    return pTable ? pTable->frameCount : 0;
}

void QmlSynthProxy::selectWavetable(const QString& group, int index) {
    Synth* pSynth = synthForGroup(group);
    if (pSynth != nullptr) {
        pSynth->selectWavetable(index);
    }
}

void QmlSynthProxy::stepWavetable(const QString& group, int delta) {
    Synth* pSynth = synthForGroup(group);
    if (pSynth == nullptr) {
        return;
    }
    const int count = pSynth->wavetableCount();
    if (count <= 0) {
        return;
    }
    const int next = ((pSynth->currentWavetable() + delta) % count + count) % count;
    pSynth->selectWavetable(next);
}

double QmlSynthProxy::lfoValue(int shape, double phase) const {
    return EngineSynth::lfoValue(shape, phase);
}

double QmlSynthProxy::envelopeSeconds(double param, int stage) const {
    const double maxSeconds = stage == 0 ? EngineSynth::kMaxAttackSeconds
            : stage == 1                 ? EngineSynth::kMaxDecaySeconds
                                         : EngineSynth::kMaxReleaseSeconds;
    return EngineSynth::envelopeSeconds(param, maxSeconds);
}

double QmlSynthProxy::cutoffHz(double param) const {
    return EngineSynth::cutoffHz(param);
}

double QmlSynthProxy::filterResponseDb(
        double cutoffParam, double resonanceParam, double hz) const {
    return EngineSynth::filterResponseDb(cutoffParam, resonanceParam, hz);
}

int QmlSynthProxy::factoryPatchCount(const QString& group) const {
    Synth* pSynth = synthForGroup(group);
    return pSynth && pSynth->patchBank() ? pSynth->patchBank()->factoryCount() : 0;
}

QString QmlSynthProxy::factoryPatchName(const QString& group, int index) const {
    Synth* pSynth = synthForGroup(group);
    return pSynth && pSynth->patchBank() ? pSynth->patchBank()->factoryName(index) : QString();
}

void QmlSynthProxy::applyFactoryPatch(const QString& group, int index) {
    Synth* pSynth = synthForGroup(group);
    if (pSynth && pSynth->patchBank()) {
        pSynth->patchBank()->applyFactory(index);
    }
}

void QmlSynthProxy::rescanWavetables(const QString& group) {
    Synth* pSynth = synthForGroup(group);
    if (pSynth != nullptr) {
        pSynth->rescanWavetables();
    }
}

// static
QmlSynthProxy* QmlSynthProxy::create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine) {
    Q_UNUSED(pJsEngine);
    // As QmlEffectsManagerProxy::create: the instance has to exist before
    // QML asks for it, and it is never replaced.
    VERIFY_OR_DEBUG_ASSERT(s_pPlayerManager) {
        qWarning() << "PlayerManager hasn't been registered for Synth yet";
        return nullptr;
    }
    return new QmlSynthProxy(s_pPlayerManager, pQmlEngine);
}

} // namespace qml
} // namespace mixxx
