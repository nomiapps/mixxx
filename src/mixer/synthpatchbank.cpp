#include "mixer/synthpatchbank.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtDebug>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "moc_synthpatchbank.cpp"

namespace {

const QString kFileName = QStringLiteral("synth-patches.json");
const QString kVersionKey = QStringLiteral("version");
const QString kGroupKey = QStringLiteral("group");
const QString kSlotsKey = QStringLiteral("slots");
constexpr int kFileVersion = 1;
constexpr int kDefaultSlot = 1;

// The sound: everything on EngineSynth that shapes it, plus the table the
// player-side Synth has chosen. Not note events, not the keyboard's key,
// scale and octave, not the channel's routing or fader, not the read-only
// wt_frames and lfo_phase.
const char* const kPatchKeys[] = {
        "osc1_wave",
        "osc2_wave",
        "osc_mix",
        "wt_position",
        "osc2_semitones",
        "osc2_detune",
        "attack",
        "decay",
        "sustain",
        "release",
        "cutoff",
        "resonance",
        "env_amount",
        "pregain",
        "lfo_shape",
        "lfo_rate",
        "lfo_sync",
        "lfo_depth",
        "lfo_target",
        "unison_voices",
        "unison_detune",
        "unison_spread",
        "wavetable",
};

int clampSlot(double v) {
    return std::clamp(static_cast<int>(std::lround(v)), 1, SynthPatchBank::kSlots);
}

} // namespace

SynthPatchBank::SynthPatchBank(const QString& group,
        UserSettingsPointer pConfig,
        const QString& filePathOverride)
        : m_group(group),
          m_filePath(filePathOverride.isEmpty()
                          ? QDir(pConfig->getSettingsPath()).filePath(kFileName)
                          : filePathOverride),
          m_currentSlot(kDefaultSlot) {
    for (const char* key : kPatchKeys) {
        m_liveKeys.append(ConfigKey(group, QLatin1String(key)));
    }

    m_pPatch = new ControlObject(ConfigKey(group, QStringLiteral("patch")),
            /*bIgnoreNops*/ true,
            /*bTrack*/ false,
            /*bPersist*/ true,
            kDefaultSlot);
    // ControlPushButton's second argument is bPersist, not a parent.
    m_pSave = new ControlPushButton(
            ConfigKey(group, QStringLiteral("patch_save")), /*bPersist*/ false);
    m_pLoad = new ControlPushButton(
            ConfigKey(group, QStringLiteral("patch_load")), /*bPersist*/ false);
    for (int i = 0; i < kSlots; ++i) {
        m_filled[i] = new ControlObject(
                ConfigKey(group, QStringLiteral("patch_%1_filled").arg(i + 1)));
        m_filled[i]->setReadOnly();
    }

    readFile();
    m_currentSlot = clampSlot(m_pPatch->get());
    publishFilled();
    // The timbre controls do not persist on their own (see the class
    // comment): the slot the synth was left on is what it sounded like.
    if (isFilled(m_currentSlot)) {
        applyToLive(m_slots[m_currentSlot - 1]);
    }

    // AutoConnection on purpose: these slots touch files (class comment).
    connect(m_pPatch, &ControlObject::valueChanged, this, &SynthPatchBank::slotPatchChanged);
    connect(m_pSave, &ControlObject::valueChanged, this, &SynthPatchBank::slotSave);
    connect(m_pLoad, &ControlObject::valueChanged, this, &SynthPatchBank::slotLoad);
}

SynthPatchBank::~SynthPatchBank() {
    for (int i = kSlots - 1; i >= 0; --i) {
        delete m_filled[i];
    }
    delete m_pLoad;
    delete m_pSave;
    delete m_pPatch;
}

bool SynthPatchBank::isFilled(int slot) const {
    return slot >= 1 && slot <= kSlots && !m_slots[slot - 1].isEmpty();
}

bool SynthPatchBank::saveSlot(int slot) {
    if (slot < 1 || slot > kSlots) {
        return false;
    }
    m_slots[slot - 1] = captureLive();
    publishFilled();
    return writeFile();
}

bool SynthPatchBank::loadSlot(int slot) {
    if (!isFilled(slot)) {
        return false;
    }
    applyToLive(m_slots[slot - 1]);
    return true;
}

void SynthPatchBank::slotPatchChanged(double v) {
    const int slot = clampSlot(v);
    if (slot == m_currentSlot) {
        return;
    }
    m_currentSlot = slot;
    if (isFilled(slot)) {
        loadSlot(slot);
    }
}

void SynthPatchBank::slotSave(double v) {
    if (v > 0.0) {
        saveSlot(m_currentSlot);
    }
}

void SynthPatchBank::slotLoad(double v) {
    if (v > 0.0) {
        loadSlot(m_currentSlot);
    }
}

QJsonObject SynthPatchBank::captureLive() const {
    QJsonObject slot;
    for (const ConfigKey& key : m_liveKeys) {
        slot.insert(key.item, ControlObject::get(key));
    }
    return slot;
}

void SynthPatchBank::applyToLive(const QJsonObject& slot) {
    for (const ConfigKey& key : m_liveKeys) {
        if (!slot.contains(key.item)) {
            continue; // an older file: leave the live value alone
        }
        double value = slot.value(key.item).toDouble();
        // Potmeters clamp themselves; the plain controls do not, and a file
        // can say anything.
        if (key.item == QLatin1String("osc1_wave") || key.item == QLatin1String("osc2_wave") ||
                key.item == QLatin1String("lfo_shape")) {
            value = std::clamp(std::round(value), 0.0, 4.0);
        } else if (key.item == QLatin1String("lfo_target")) {
            value = std::clamp(std::round(value), 0.0, 3.0);
        } else if (key.item == QLatin1String("unison_voices")) {
            value = std::clamp(std::round(value), 1.0, 4.0);
        } else if (key.item == QLatin1String("lfo_sync")) {
            value = value > 0.0 ? 1.0 : 0.0;
        } else if (key.item == QLatin1String("wavetable")) {
            value = std::max(0.0, std::round(value)); // the Synth clamps the top
        }
        ControlObject::set(key, value);
    }
}

void SynthPatchBank::readFile() {
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Ignoring synth patches in" << m_filePath << ":" << error.errorString();
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(kVersionKey).toInt() != kFileVersion) {
        qWarning() << "Ignoring synth patches in" << m_filePath
                   << ": unknown version" << root.value(kVersionKey);
        return;
    }
    const QJsonObject slotObjects = root.value(kSlotsKey).toObject();
    for (int i = 0; i < kSlots; ++i) {
        m_slots[i] = slotObjects.value(QString::number(i + 1)).toObject();
    }
}

bool SynthPatchBank::writeFile() const {
    QJsonObject slotObjects; // not "slots": that is a Qt keyword
    for (int i = 0; i < kSlots; ++i) {
        if (!m_slots[i].isEmpty()) {
            slotObjects.insert(QString::number(i + 1), m_slots[i]);
        }
    }
    QJsonObject root;
    root.insert(kVersionKey, kFileVersion);
    root.insert(kGroupKey, m_group);
    root.insert(kSlotsKey, slotObjects);

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to save synth patches to" << m_filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void SynthPatchBank::publishFilled() {
    for (int i = 0; i < kSlots; ++i) {
        m_filled[i]->setAndConfirm(isFilled(i + 1) ? 1.0 : 0.0);
    }
}
