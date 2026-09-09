#include "engine/sequencerpatternbank.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtDebug>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "engine/enginesequencer.h"
#include "moc_sequencerpatternbank.cpp"

namespace {

const QString kFileName = QStringLiteral("sequencer-patterns.json");
const QString kVersionKey = QStringLiteral("version");
const QString kGroupKey = QStringLiteral("group");
const QString kSlotsKey = QStringLiteral("slots");
constexpr int kFileVersion = 1;
constexpr int kDefaultSlot = 1;

int clampSlot(double v) {
    return std::clamp(static_cast<int>(std::lround(v)), 1, SequencerPatternBank::kSlots);
}

} // namespace

SequencerPatternBank::SequencerPatternBank(const QString& group,
        UserSettingsPointer pConfig,
        const QString& filePathOverride)
        : m_group(group),
          m_filePath(filePathOverride.isEmpty()
                          ? QDir(pConfig->getSettingsPath()).filePath(kFileName)
                          : filePathOverride),
          m_currentSlot(kDefaultSlot) {
    m_liveKeys.append(ConfigKey(group, QStringLiteral("length")));
    m_liveKeys.append(ConfigKey(group, QStringLiteral("swing")));
    for (int step = 1; step <= EngineSequencer::kSteps; ++step) {
        for (const char* field : {"enabled", "note", "velocity", "gate"}) {
            m_liveKeys.append(ConfigKey(group,
                    QStringLiteral("synth_step_%1_%2").arg(step).arg(QLatin1String(field))));
        }
        for (int lane = 1; lane <= EngineSequencer::kSamplerLanes; ++lane) {
            m_liveKeys.append(ConfigKey(group,
                    QStringLiteral("sampler_%1_step_%2_enabled").arg(lane).arg(step)));
        }
    }
    for (int lane = 1; lane <= EngineSequencer::kSamplerLanes; ++lane) {
        m_liveKeys.append(ConfigKey(group, QStringLiteral("sampler_%1_target").arg(lane)));
    }

    m_pPattern = new ControlObject(ConfigKey(group, QStringLiteral("pattern")),
            /*bIgnoreNops*/ true,
            /*bTrack*/ false,
            /*bPersist*/ true,
            kDefaultSlot);
    // ControlPushButton's second argument is bPersist, not a parent. The
    // buttons stay out of the config.
    m_pSave = new ControlPushButton(
            ConfigKey(group, QStringLiteral("pattern_save")), /*bPersist*/ false);
    m_pLoad = new ControlPushButton(
            ConfigKey(group, QStringLiteral("pattern_load")), /*bPersist*/ false);
    for (int i = 0; i < kSlots; ++i) {
        m_filled[i] = new ControlObject(
                ConfigKey(group, QStringLiteral("pattern_%1_filled").arg(i + 1)));
        m_filled[i]->setReadOnly();
    }

    readFile();
    m_currentSlot = clampSlot(m_pPattern->get());
    publishFilled();

    // AutoConnection on purpose: these slots touch files (class comment).
    connect(m_pPattern,
            &ControlObject::valueChanged,
            this,
            &SequencerPatternBank::slotPatternChanged);
    connect(m_pSave, &ControlObject::valueChanged, this, &SequencerPatternBank::slotSave);
    connect(m_pLoad, &ControlObject::valueChanged, this, &SequencerPatternBank::slotLoad);
}

SequencerPatternBank::~SequencerPatternBank() {
    for (int i = kSlots - 1; i >= 0; --i) {
        delete m_filled[i];
    }
    delete m_pLoad;
    delete m_pSave;
    delete m_pPattern;
}

bool SequencerPatternBank::isFilled(int slot) const {
    return slot >= 1 && slot <= kSlots && !m_slots[slot - 1].isEmpty();
}

bool SequencerPatternBank::saveSlot(int slot) {
    if (slot < 1 || slot > kSlots) {
        return false;
    }
    m_slots[slot - 1] = captureLive();
    publishFilled();
    return writeFile();
}

bool SequencerPatternBank::loadSlot(int slot) {
    if (!isFilled(slot)) {
        return false;
    }
    applyToLive(m_slots[slot - 1]);
    return true;
}

void SequencerPatternBank::slotPatternChanged(double v) {
    const int slot = clampSlot(v);
    if (slot == m_currentSlot) {
        return;
    }
    m_currentSlot = slot;
    if (isFilled(slot)) {
        loadSlot(slot);
    }
}

void SequencerPatternBank::slotSave(double v) {
    if (v > 0.0) {
        saveSlot(m_currentSlot);
    }
}

void SequencerPatternBank::slotLoad(double v) {
    if (v > 0.0) {
        loadSlot(m_currentSlot);
    }
}

QJsonObject SequencerPatternBank::captureLive() const {
    QJsonObject slot;
    for (const ConfigKey& key : m_liveKeys) {
        slot.insert(key.item, ControlObject::get(key));
    }
    return slot;
}

void SequencerPatternBank::applyToLive(const QJsonObject& slot) {
    for (const ConfigKey& key : m_liveKeys) {
        if (!slot.contains(key.item)) {
            continue; // an older file: leave the live value alone
        }
        double value = slot.value(key.item).toDouble();
        // Potmeters clamp themselves; the plain controls do not.
        if (key.item == QLatin1String("length")) {
            value = std::clamp(value, 1.0, static_cast<double>(EngineSequencer::kSteps));
        } else if (key.item.endsWith(QLatin1String("_note"))) {
            value = std::clamp(value, 0.0, 127.0);
        } else if (key.item.endsWith(QLatin1String("_target"))) {
            value = std::clamp(value, 1.0, static_cast<double>(EngineSequencer::kMaxSamplerTargets));
        }
        ControlObject::set(key, value);
    }
}

void SequencerPatternBank::readFile() {
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Ignoring sequencer patterns in" << m_filePath << ":"
                   << error.errorString();
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(kVersionKey).toInt() != kFileVersion) {
        qWarning() << "Ignoring sequencer patterns in" << m_filePath
                   << ": unknown version" << root.value(kVersionKey);
        return;
    }
    const QJsonObject slotObjects = root.value(kSlotsKey).toObject();
    for (int i = 0; i < kSlots; ++i) {
        m_slots[i] = slotObjects.value(QString::number(i + 1)).toObject();
    }
}

bool SequencerPatternBank::writeFile() const {
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
        qWarning() << "Failed to save sequencer patterns to" << m_filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void SequencerPatternBank::publishFilled() {
    for (int i = 0; i < kSlots; ++i) {
        m_filled[i]->setAndConfirm(isFilled(i + 1) ? 1.0 : 0.0);
    }
}
