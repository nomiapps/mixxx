#include "effects/backends/clap/clapeffectprocessor.h"

#include "engine/effects/engineeffectparameter.h"
#include "util/defs.h"
#include "util/sample.h"

namespace {

constexpr double kSecondsPerMinute = 60.0;
constexpr int64_t kNoSteadyTime = -1;

bool CLAP_ABI discardOutputEvent(const clap_output_events*, const clap_event_header*) {
    return true;
}

} // anonymous namespace

CLAPEffectGroupState::CLAPEffectGroupState(
        const mixxx::EngineParameters& engineParameters)
        : EffectState(engineParameters),
          m_sampleRate(engineParameters.sampleRate()),
          m_eventCount(0),
          m_sendAllValues(true),
          m_inputL(kMaxEngineFrames),
          m_inputR(kMaxEngineFrames),
          m_outputL(kMaxEngineFrames),
          m_outputR(kMaxEngineFrames),
          m_silence(kMaxEngineFrames),
          m_discarded(kMaxEngineFrames) {
}

namespace {

/// Lays out the buffers of every port of one direction: the first port gets
/// the two given channels, each channel of the others gets pOther.
void connectPorts(const QList<uint32_t>& portChannels,
        float* pLeft,
        float* pRight,
        float* pOther,
        bool otherIsSilent,
        std::vector<std::vector<float*>>* pChannels,
        std::vector<clap_audio_buffer>* pPorts) {
    pChannels->clear();
    pPorts->clear();
    for (const uint32_t channelCount : portChannels) {
        if (pChannels->empty()) {
            pChannels->push_back({pLeft, pRight});
        } else {
            pChannels->push_back(std::vector<float*>(channelCount, pOther));
        }
    }
    for (std::vector<float*>& channels : *pChannels) {
        const bool isMainPort = pPorts->empty();
        clap_audio_buffer port = {};
        port.data32 = channels.data();
        port.channel_count = static_cast<uint32_t>(channels.size());
        if (!isMainPort && otherIsSilent) {
            // Tells the plugin that every channel of this port is constant.
            port.constant_mask = ~uint64_t{0};
        }
        pPorts->push_back(port);
    }
}

} // anonymous namespace

void CLAPEffectGroupState::load(const CLAPManifestPointer& pManifest) {
    connectPorts(pManifest->inputPortChannels(),
            m_inputL.data(),
            m_inputR.data(),
            m_silence.data(),
            true,
            &m_inputPortChannels,
            &m_inputPorts);
    connectPorts(pManifest->outputPortChannels(),
            m_outputL.data(),
            m_outputR.data(),
            m_discarded.data(),
            false,
            &m_outputPortChannels,
            &m_outputPorts);
    m_parameterIds = pManifest->parameterIds();
    m_fixedParameters = pManifest->fixedParameters();
    m_sentValues.assign(m_parameterIds.size(), 0.0);
    m_sendAllValues = true;
    m_events.resize(m_parameterIds.size() + m_fixedParameters.size());
    m_pInstance = CLAPPluginInstance::create(pManifest->library(), pManifest->pluginId());
    if (m_pInstance && !m_pInstance->activate(m_sampleRate.toDouble(), kMaxEngineFrames)) {
        qWarning() << "CLAPEffectGroupState: could not activate" << pManifest->name();
        m_pInstance.reset();
    }
}

void CLAPEffectGroupState::setParameter(int index, double value) {
    if (!m_sendAllValues && m_sentValues[index] == value) {
        return;
    }
    m_sentValues[index] = value;
    queueParameterValue(m_parameterIds[index], value);
}

void CLAPEffectGroupState::queueParameterValue(clap_id parameterId, double value) {
    clap_event_param_value& event = m_events[m_eventCount++];
    event = {};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = parameterId;
    // -1 addresses every note, port, channel and key.
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = value;
}

// static
uint32_t CLAP_ABI CLAPEffectGroupState::eventCount(const clap_input_events* pList) {
    return static_cast<const CLAPEffectGroupState*>(pList->ctx)->m_eventCount;
}

// static
const clap_event_header* CLAP_ABI CLAPEffectGroupState::eventAt(
        const clap_input_events* pList, uint32_t index) {
    const auto* pState = static_cast<const CLAPEffectGroupState*>(pList->ctx);
    if (index >= pState->m_eventCount) {
        return nullptr;
    }
    return &pState->m_events[index].header;
}

bool CLAPEffectGroupState::process(const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        SINT frames,
        const GroupFeatureState& groupFeatures) {
    const clap_plugin* pPlugin = m_pInstance->plugin();

    if (m_sendAllValues) {
        for (const CLAPManifest::FixedParameter& fixed : std::as_const(m_fixedParameters)) {
            queueParameterValue(fixed.id, fixed.value);
        }
    }

    // note: LOOP VECTORIZED.
    for (SINT i = 0; i < frames; ++i) {
        m_inputL[i] = pInput[i * 2];
        m_inputR[i] = pInput[i * 2 + 1];
    }
    const clap_input_events inputEvents = {this, eventCount, eventAt};
    const clap_output_events outputEvents = {nullptr, discardOutputEvent};

    // The tempo of the deck being processed, for plugins that follow it.
    clap_event_transport transport = {};
    transport.header.size = sizeof(transport);
    transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transport.header.type = CLAP_EVENT_TRANSPORT;
    const bool hasTempo = groupFeatures.beat_length.has_value() &&
            groupFeatures.beat_length->seconds > 0.0;
    if (hasTempo) {
        transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;
        transport.tempo = kSecondsPerMinute / groupFeatures.beat_length->seconds;
    }

    clap_process process = {};
    process.steady_time = kNoSteadyTime;
    process.frames_count = static_cast<uint32_t>(frames);
    process.transport = hasTempo ? &transport : nullptr;
    process.audio_inputs = m_inputPorts.data();
    process.audio_outputs = m_outputPorts.data();
    process.audio_inputs_count = static_cast<uint32_t>(m_inputPorts.size());
    process.audio_outputs_count = static_cast<uint32_t>(m_outputPorts.size());
    process.in_events = &inputEvents;
    process.out_events = &outputEvents;

    const clap_process_status status = pPlugin->process(pPlugin, &process);
    m_eventCount = 0;
    m_sendAllValues = false;
    if (status == CLAP_PROCESS_ERROR) {
        return false;
    }

    // note: LOOP VECTORIZED.
    for (SINT i = 0; i < frames; ++i) {
        pOutput[i * 2] = m_outputL[i];
        pOutput[i * 2 + 1] = m_outputR[i];
    }
    return true;
}

CLAPEffectProcessor::CLAPEffectProcessor(CLAPManifestPointer pManifest)
        : m_pManifest(std::move(pManifest)),
          m_groupDelayFrames(0) {
}

void CLAPEffectProcessor::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    // Keep the parameters in the order of the manifest, so that the engine
    // thread reaches each one by index and compares no strings.
    for (const auto& pManifestParameter : m_pManifest->parameters()) {
        m_engineEffectParameters.append(parameters.value(pManifestParameter->id()));
    }
}

void CLAPEffectProcessor::processChannel(
        CLAPEffectGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    CLAPPluginInstance* pInstance = pState->instance();
    const SINT frames = engineParameters.framesPerBuffer();
    // A plugin is activated for one sample rate, on the main thread. If the
    // engine's rate has changed since, the signal passes through unchanged
    // until the effect is loaded again.
    if (!pInstance ||
            pState->sampleRate() != engineParameters.sampleRate() ||
            frames > static_cast<SINT>(kMaxEngineFrames) ||
            !pInstance->startProcessing()) {
        SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
        return;
    }

    if (enableState == EffectEnableState::Enabling) {
        // Drop whatever tail was left from before the effect was switched off.
        pInstance->plugin()->reset(pInstance->plugin());
    }

    for (int i = 0; i < m_engineEffectParameters.size(); ++i) {
        pState->setParameter(i, m_engineEffectParameters[i]->value());
    }

    if (!pState->process(pInput, pOutput, frames, groupFeatures)) {
        SampleUtil::copy(pOutput, pInput, engineParameters.samplesPerBuffer());
    }
}

CLAPEffectGroupState* CLAPEffectProcessor::createSpecificState(
        const mixxx::EngineParameters& engineParameters) {
    auto* pState = new CLAPEffectGroupState(engineParameters);
    pState->load(m_pManifest);
    if (pState->instance()) {
        m_groupDelayFrames.store(
                static_cast<SINT>(pState->instance()->latencyFrames()),
                std::memory_order_relaxed);
    }
    return pState;
}
