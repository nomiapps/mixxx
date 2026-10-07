#include "effects/backends/clap/clapmanifest.h"

#include "effects/defs.h"

namespace {

constexpr uint32_t kStereoChannels = 2;
constexpr uint32_t kUnusableParameterFlags =
        CLAP_PARAM_IS_HIDDEN | CLAP_PARAM_IS_READONLY | CLAP_PARAM_IS_BYPASS;

/// Returns the channel count of each input or output port, or an empty
/// list if the plugin does not describe its ports.
QList<uint32_t> portChannels(const CLAPPluginInstance& instance, bool isInput) {
    QList<uint32_t> channels;
    const auto* pPorts = instance.extension<clap_plugin_audio_ports>(CLAP_EXT_AUDIO_PORTS);
    if (!pPorts || !pPorts->count || !pPorts->get) {
        return channels;
    }
    const uint32_t count = pPorts->count(instance.plugin(), isInput);
    for (uint32_t i = 0; i < count; ++i) {
        clap_audio_port_info info = {};
        if (!pPorts->get(instance.plugin(), i, isInput, &info)) {
            return {};
        }
        channels.append(info.channel_count);
    }
    return channels;
}

/// Mixxx routes one stereo signal through an effect, so the first input
/// port and the first output port must be stereo.
bool startsWithStereoPort(const QList<uint32_t>& portChannels) {
    return !portChannels.isEmpty() && portChannels.first() == kStereoChannels;
}

QString valueText(const CLAPPluginInstance& instance,
        const clap_plugin_params* pParams,
        clap_id parameterId,
        double value) {
    char text[CLAP_NAME_SIZE] = {};
    if (pParams->value_to_text &&
            pParams->value_to_text(instance.plugin(),
                    parameterId,
                    value,
                    text,
                    sizeof(text))) {
        return QString::fromUtf8(text);
    }
    return QString::number(value);
}

} // anonymous namespace

CLAPManifest::CLAPManifest(CLAPLibraryPointer pLibrary,
        const clap_plugin_descriptor* pDescriptor)
        : m_pLibrary(std::move(pLibrary)),
          m_pluginId(pDescriptor->id),
          m_status(Status::Available) {
    setBackendType(EffectBackendType::CLAP);
    setId(QString::fromUtf8(pDescriptor->id));
    setName(QString::fromUtf8(pDescriptor->name));
    setAuthor(QString::fromUtf8(pDescriptor->vendor));
    setVersion(QString::fromUtf8(pDescriptor->version));
    setDescription(QString::fromUtf8(pDescriptor->description));

    const auto pInstance = CLAPPluginInstance::create(m_pLibrary, m_pluginId);
    if (!pInstance) {
        m_status = Status::NotInstantiable;
        return;
    }
    m_inputPortChannels = portChannels(*pInstance, true);
    m_outputPortChannels = portChannels(*pInstance, false);
    if (!startsWithStereoPort(m_inputPortChannels) ||
            !startsWithStereoPort(m_outputPortChannels)) {
        m_status = Status::IoNotStereo;
        return;
    }
    readParameters(*pInstance);
}

void CLAPManifest::readParameters(const CLAPPluginInstance& instance) {
    const auto* pParams = instance.extension<clap_plugin_params>(CLAP_EXT_PARAMS);
    if (!pParams || !pParams->count || !pParams->get_info) {
        return;
    }
    const uint32_t count = pParams->count(instance.plugin());
    for (uint32_t i = 0; i < count; ++i) {
        clap_param_info info = {};
        if (!pParams->get_info(instance.plugin(), i, &info)) {
            continue;
        }
        if ((info.flags & kUnusableParameterFlags) != 0 ||
                !(info.min_value < info.max_value)) {
            continue;
        }
        const double defaultValue = qBound(info.min_value, info.default_value, info.max_value);

        EffectManifestParameterPointer pParameter = addParameter();
        m_parameterIds.append(info.id);
        pParameter->setId(QString::number(info.id));
        const QString name = QString::fromUtf8(info.name);
        pParameter->setName(name);
        pParameter->setShortName(name);
        pParameter->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
        pParameter->setRange(info.min_value, defaultValue, info.max_value);

        const bool stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
        if (stepped && info.max_value - info.min_value == 1.0) {
            // A two-state parameter is a button, labelled by the plugin.
            pParameter->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
            for (const double value : {info.min_value, info.max_value}) {
                pParameter->appendStep(qMakePair(
                        valueText(instance, pParams, info.id, value), value));
            }
        } else if (stepped) {
            pParameter->setValueScaler(EffectManifestParameter::ValueScaler::Integral);
        } else {
            pParameter->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
        }
    }
}
