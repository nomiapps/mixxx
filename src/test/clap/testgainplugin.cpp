// A minimal CLAP plugin for CLAPBackendTest: a stereo gain with an invert
// switch and its own dry/wet mix, and a sidechain input that is added to the output so that a test
// can tell the host fed it silence. It is built as its own module and loaded
// like an installed plugin.

#include <clap/clap.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr char kGainPluginId[] = "org.mixxx.test.clap-gain";
constexpr char kInstrumentPluginId[] = "org.mixxx.test.clap-instrument";

constexpr clap_id kGainParamId = 7;
constexpr clap_id kInvertParamId = 9;
constexpr clap_id kHiddenParamId = 11;
constexpr clap_id kMixParamId = 13;
constexpr uint32_t kParamCount = 4;
constexpr double kMixMax = 100.0;
constexpr double kMixDefault = 30.0;
constexpr double kGainMin = 0.0;
constexpr double kGainDefault = 1.0;
constexpr double kGainMax = 2.0;
constexpr uint32_t kLatencyFrames = 3;

struct GainPlugin {
    clap_plugin plugin;
    double gain = kGainDefault;
    bool invert = false;
    double mix = kMixDefault;
};

GainPlugin* self(const clap_plugin* pPlugin) {
    return static_cast<GainPlugin*>(pPlugin->plugin_data);
}

// Parameters

uint32_t CLAP_ABI paramsCount(const clap_plugin*) {
    return kParamCount;
}

bool CLAP_ABI paramsGetInfo(const clap_plugin*, uint32_t index, clap_param_info* pInfo) {
    *pInfo = {};
    switch (index) {
    case 0:
        pInfo->id = kGainParamId;
        pInfo->flags = CLAP_PARAM_IS_AUTOMATABLE;
        std::snprintf(pInfo->name, sizeof(pInfo->name), "Gain");
        pInfo->min_value = kGainMin;
        pInfo->max_value = kGainMax;
        pInfo->default_value = kGainDefault;
        return true;
    case 1:
        pInfo->id = kInvertParamId;
        pInfo->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
        std::snprintf(pInfo->name, sizeof(pInfo->name), "Invert");
        pInfo->min_value = 0.0;
        pInfo->max_value = 1.0;
        pInfo->default_value = 0.0;
        return true;
    case 2:
        pInfo->id = kHiddenParamId;
        pInfo->flags = CLAP_PARAM_IS_HIDDEN;
        std::snprintf(pInfo->name, sizeof(pInfo->name), "Hidden");
        pInfo->min_value = 0.0;
        pInfo->max_value = 1.0;
        pInfo->default_value = 0.0;
        return true;
    case 3:
        pInfo->id = kMixParamId;
        pInfo->flags = CLAP_PARAM_IS_AUTOMATABLE;
        std::snprintf(pInfo->name, sizeof(pInfo->name), "Mix");
        pInfo->min_value = 0.0;
        pInfo->max_value = kMixMax;
        pInfo->default_value = kMixDefault;
        return true;
    default:
        return false;
    }
}

bool CLAP_ABI paramsGetValue(const clap_plugin* pPlugin, clap_id id, double* pValue) {
    if (id == kGainParamId) {
        *pValue = self(pPlugin)->gain;
        return true;
    }
    if (id == kInvertParamId) {
        *pValue = self(pPlugin)->invert ? 1.0 : 0.0;
        return true;
    }
    if (id == kMixParamId) {
        *pValue = self(pPlugin)->mix;
        return true;
    }
    return false;
}

bool CLAP_ABI paramsValueToText(
        const clap_plugin*, clap_id id, double value, char* pText, uint32_t size) {
    if (id == kInvertParamId) {
        std::snprintf(pText, size, "%s", value > 0.5 ? "Inverted" : "Normal");
    } else {
        std::snprintf(pText, size, "%.2f", value);
    }
    return true;
}

bool CLAP_ABI paramsTextToValue(const clap_plugin*, clap_id, const char*, double*) {
    return false;
}

void applyEvents(GainPlugin* pSelf, const clap_input_events* pEvents) {
    const uint32_t count = pEvents->size(pEvents);
    for (uint32_t i = 0; i < count; ++i) {
        const clap_event_header* pHeader = pEvents->get(pEvents, i);
        if (pHeader->space_id != CLAP_CORE_EVENT_SPACE_ID ||
                pHeader->type != CLAP_EVENT_PARAM_VALUE) {
            continue;
        }
        const auto* pEvent = reinterpret_cast<const clap_event_param_value*>(pHeader);
        if (pEvent->param_id == kGainParamId) {
            pSelf->gain = pEvent->value;
        } else if (pEvent->param_id == kInvertParamId) {
            pSelf->invert = pEvent->value > 0.5;
        } else if (pEvent->param_id == kMixParamId) {
            pSelf->mix = pEvent->value;
        }
    }
}

void CLAP_ABI paramsFlush(const clap_plugin* pPlugin,
        const clap_input_events* pIn,
        const clap_output_events*) {
    applyEvents(self(pPlugin), pIn);
}

const clap_plugin_params kParams = {
        paramsCount,
        paramsGetInfo,
        paramsGetValue,
        paramsValueToText,
        paramsTextToValue,
        paramsFlush,
};

// Audio ports

constexpr uint32_t kSidechainPort = 1;

uint32_t CLAP_ABI portsCount(const clap_plugin*, bool isInput) {
    return isInput ? 2 : 1;
}

bool CLAP_ABI portsGet(
        const clap_plugin*, uint32_t index, bool isInput, clap_audio_port_info* pInfo) {
    if (index >= portsCount(nullptr, isInput)) {
        return false;
    }
    *pInfo = {};
    pInfo->id = index;
    std::snprintf(pInfo->name,
            sizeof(pInfo->name),
            "%s",
            index == kSidechainPort ? "Sidechain" : "Main");
    pInfo->flags = index == kSidechainPort ? 0 : CLAP_AUDIO_PORT_IS_MAIN;
    pInfo->channel_count = 2;
    pInfo->port_type = CLAP_PORT_STEREO;
    pInfo->in_place_pair = CLAP_INVALID_ID;
    return true;
}

const clap_plugin_audio_ports kAudioPorts = {portsCount, portsGet};

// Latency

uint32_t CLAP_ABI latencyGet(const clap_plugin*) {
    return kLatencyFrames;
}

const clap_plugin_latency kLatency = {latencyGet};

// Plugin

bool CLAP_ABI pluginInit(const clap_plugin*) {
    return true;
}

void CLAP_ABI pluginDestroy(const clap_plugin* pPlugin) {
    delete self(pPlugin);
}

bool CLAP_ABI pluginActivate(const clap_plugin*, double, uint32_t, uint32_t) {
    return true;
}

void CLAP_ABI pluginDeactivate(const clap_plugin*) {
}

bool CLAP_ABI pluginStartProcessing(const clap_plugin*) {
    return true;
}

void CLAP_ABI pluginStopProcessing(const clap_plugin*) {
}

void CLAP_ABI pluginReset(const clap_plugin*) {
}

clap_process_status CLAP_ABI pluginProcess(
        const clap_plugin* pPlugin, const clap_process* pProcess) {
    GainPlugin* pSelf = self(pPlugin);
    applyEvents(pSelf, pProcess->in_events);
    const float gain = static_cast<float>(pSelf->invert ? -pSelf->gain : pSelf->gain);
    const float wet = static_cast<float>(pSelf->mix / kMixMax);
    for (uint32_t channel = 0; channel < 2; ++channel) {
        const float* pIn = pProcess->audio_inputs[0].data32[channel];
        const float* pSidechain = pProcess->audio_inputs[kSidechainPort].data32[channel];
        float* pOut = pProcess->audio_outputs[0].data32[channel];
        for (uint32_t i = 0; i < pProcess->frames_count; ++i) {
            pOut[i] = pIn[i] * (1.0f - wet) + pIn[i] * gain * wet + pSidechain[i];
        }
    }
    return CLAP_PROCESS_CONTINUE;
}

const void* CLAP_ABI pluginGetExtension(const clap_plugin*, const char* pId) {
    if (std::strcmp(pId, CLAP_EXT_PARAMS) == 0) {
        return &kParams;
    }
    if (std::strcmp(pId, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &kAudioPorts;
    }
    if (std::strcmp(pId, CLAP_EXT_LATENCY) == 0) {
        return &kLatency;
    }
    return nullptr;
}

void CLAP_ABI pluginOnMainThread(const clap_plugin*) {
}

const char* const kGainFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, nullptr};
const char* const kInstrumentFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, nullptr};

const clap_plugin_descriptor kDescriptors[] = {
        {
                CLAP_VERSION,
                kGainPluginId,
                "Test Gain",
                "Mixxx",
                "",
                "",
                "",
                "1.0",
                "Gain with an invert switch",
                kGainFeatures,
        },
        {
                CLAP_VERSION,
                kInstrumentPluginId,
                "Test Instrument",
                "Mixxx",
                "",
                "",
                "",
                "1.0",
                "Not an audio effect, so Mixxx must leave it out",
                kInstrumentFeatures,
        },
};
constexpr uint32_t kDescriptorCount = sizeof(kDescriptors) / sizeof(kDescriptors[0]);

// Factory and entry

uint32_t CLAP_ABI factoryCount(const clap_plugin_factory*) {
    return kDescriptorCount;
}

const clap_plugin_descriptor* CLAP_ABI factoryDescriptor(
        const clap_plugin_factory*, uint32_t index) {
    return index < kDescriptorCount ? &kDescriptors[index] : nullptr;
}

const clap_plugin* CLAP_ABI factoryCreate(
        const clap_plugin_factory*, const clap_host*, const char* pId) {
    if (std::strcmp(pId, kGainPluginId) != 0) {
        return nullptr;
    }
    auto* pSelf = new GainPlugin();
    pSelf->plugin = {
            &kDescriptors[0],
            pSelf,
            pluginInit,
            pluginDestroy,
            pluginActivate,
            pluginDeactivate,
            pluginStartProcessing,
            pluginStopProcessing,
            pluginReset,
            pluginProcess,
            pluginGetExtension,
            pluginOnMainThread,
    };
    return &pSelf->plugin;
}

const clap_plugin_factory kFactory = {factoryCount, factoryDescriptor, factoryCreate};

bool CLAP_ABI entryInit(const char*) {
    return true;
}

void CLAP_ABI entryDeinit() {
}

const void* CLAP_ABI entryGetFactory(const char* pId) {
    return std::strcmp(pId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory : nullptr;
}

} // anonymous namespace

extern "C" {
CLAP_EXPORT const clap_plugin_entry clap_entry = {
        CLAP_VERSION,
        entryInit,
        entryDeinit,
        entryGetFactory,
};
}
