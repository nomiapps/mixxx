#include "mixer/synth.h"

#include <QtDebug>
#include <algorithm>
#include <cmath>
#include <memory>

#include "control/controlobject.h"
#include "engine/channels/enginesynth.h"
#include "engine/enginemixer.h"
#include "mixer/wavetablelibrary.h"
#include "moc_synth.cpp"

namespace {
/// Every note playable: no scale selected yet.
constexpr double kDefaultScaleMask = 4095.0;
/// C3, an octave either side of which is comfortable on a small keyboard.
constexpr double kDefaultBaseNote = 48.0;
} // namespace

Synth::Synth(PlayerManager* pParent,
        const QString& group,
        UserSettingsPointer pConfig,
        EngineMixer* pEngine,
        EffectsManager* pEffectsManager)
        : BasePlayer(pParent, group) {
    // The lane has to exist before the channel is in the mixer, because from
    // then on the engine end belongs to the engine thread.
    auto pipes = makeTwoWayMessagePipe<Wavetable*, Wavetable*>(
            kWavetableLaneDepth, kWavetableLaneDepth);
    m_wavetablePipe.emplace(std::move(pipes.first));

    ChannelHandleAndGroup channelGroup = pEngine->registerChannelGroup(group);
    auto pSynth = std::make_unique<EngineSynth>(channelGroup, pEffectsManager);
    pSynth->setWavetablePipe(std::move(pipes.second));
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

    // The wavetable. The list is scanned first so the persisted index has
    // something to be clamped against; selecting then loads it, which for a
    // built-in completes before the constructor returns.
    m_pWavetable = std::make_unique<ControlObject>(ConfigKey(group, QStringLiteral("wavetable")),
            true,
            false,
            /*bPersist*/ true,
            0.0);
    m_pWtFrames = std::make_unique<ControlObject>(ConfigKey(group, QStringLiteral("wt_frames")));
    m_pWtFrames->setReadOnly();

    m_pLibrary = std::make_unique<WavetableLibrary>(pConfig, this);
    connect(m_pLibrary.get(), &WavetableLibrary::loaded, this, &Synth::slotWavetableLoaded);
    connect(m_pLibrary.get(),
            &WavetableLibrary::loadFailed,
            this,
            &Synth::slotWavetableLoadFailed);
    connect(m_pLibrary.get(),
            &WavetableLibrary::namesChanged,
            this,
            &Synth::wavetableNamesChanged);
    m_pLibrary->scan();
    connect(m_pWavetable.get(),
            &ControlObject::valueChanged,
            this,
            &Synth::slotWavetableControlChanged);
    selectWavetable(static_cast<int>(std::lround(m_pWavetable->get())));
}

Synth::~Synth() {
    // Runs after the audio callback has stopped (PlayerManager goes before
    // EngineMixer), so whatever the engine has handed back by now is all it
    // ever will. The copy it is still holding is EngineSynth's to free.
    collectRetiredTables();
}

QStringList Synth::wavetableNames() const {
    return m_pLibrary->names();
}

int Synth::wavetableCount() const {
    return m_pLibrary->count();
}

int Synth::currentWavetable() const {
    const int count = m_pLibrary->count();
    if (count <= 0) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround(m_pWavetable->get())), 0, count - 1);
}

void Synth::selectWavetable(int index) {
    const int count = m_pLibrary->count();
    if (count <= 0) {
        return;
    }
    index = std::clamp(index, 0, count - 1);
    if (index != static_cast<int>(std::lround(m_pWavetable->get()))) {
        // The control change loads it, through slotWavetableControlChanged.
        m_pWavetable->set(index);
        return;
    }
    m_pLibrary->load(index);
}

void Synth::rescanWavetables() {
    m_pLibrary->scan();
    selectWavetable(currentWavetable());
}

void Synth::slotWavetableControlChanged(double value) {
    const int count = m_pLibrary->count();
    if (count <= 0) {
        return;
    }
    const int index = std::clamp(static_cast<int>(std::lround(value)), 0, count - 1);
    if (index != static_cast<int>(std::lround(value))) {
        m_pWavetable->set(index); // comes back here with the clamped value
        return;
    }
    m_pLibrary->load(index);
}

void Synth::slotWavetableLoaded(int index, std::shared_ptr<const Wavetable> pTable) {
    if (index != currentWavetable() || !pTable) {
        return; // the selection moved on while this one was decoding
    }
    m_pUiTable = pTable;
    sendToEngine(*pTable);
    m_pWtFrames->setAndConfirm(pTable->frameCount);
    emit wavetableChanged();
}

void Synth::slotWavetableLoadFailed(int index, const QString& reason) {
    if (index != currentWavetable()) {
        return;
    }
    const QString name = index >= 0 && index < m_pLibrary->count()
            ? m_pLibrary->entry(index).name
            : QString::number(index);
    qWarning() << getGroup() << "wavetable" << name << "not loaded:" << reason;
    // The engine keeps the last table it was given; only the display and the
    // frame count say that this one is not it.
    m_pUiTable.reset();
    m_pWtFrames->setAndConfirm(0.0);
    emit wavetableChanged();
}

void Synth::sendToEngine(const Wavetable& table) {
    collectRetiredTables();
    // The engine gets its own copy: the display keeps drawing from the other
    // one while this is handed over, and neither is ever shared.
    auto* pCopy = new Wavetable(table);
    if (!m_wavetablePipe->writeMessage(pCopy)) {
        // The lane is full because the engine is not running to drain it.
        // The display still shows the table; the engine gets the next one
        // chosen once it is running.
        delete pCopy;
        qWarning() << getGroup() << "wavetable not sent: the engine is not taking them";
    }
}

void Synth::collectRetiredTables() {
    Wavetable* pRetired = nullptr;
    while (m_wavetablePipe->readMessage(&pRetired)) {
        delete pRetired;
    }
}
