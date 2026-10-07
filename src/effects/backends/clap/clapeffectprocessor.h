#pragma once

#include <atomic>
#include <vector>

#include "effects/backends/clap/clapmanifest.h"
#include "effects/backends/clap/clapplugin.h"
#include "effects/backends/effectprocessor.h"
#include "effects/defs.h"
#include "engine/engine.h"

/// One activated plugin instance, with everything a process call hands to
/// it. All of it is allocated here, on the main thread.
// Refer to EffectProcessor for documentation
class CLAPEffectGroupState final : public EffectState {
  public:
    explicit CLAPEffectGroupState(const mixxx::EngineParameters& engineParameters);
    ~CLAPEffectGroupState() override = default;

    /// Creates and activates the plugin. Until then, and if that fails,
    /// instance() is null.
    void load(const CLAPManifestPointer& pManifest);

    /// Null if the plugin could not be created or activated.
    CLAPPluginInstance* instance() const {
        return m_pInstance.get();
    }
    mixxx::audio::SampleRate sampleRate() const {
        return m_sampleRate;
    }

    /// Queues a parameter change for the next process call if the value
    /// differs from the one the plugin was last given.
    void setParameter(int index, double value);

    /// Runs the plugin over one buffer of interleaved stereo samples.
    /// Returns false if the plugin failed, leaving pOutput untouched.
    bool process(const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            SINT frames,
            const GroupFeatureState& groupFeatures);

  private:
    static uint32_t CLAP_ABI eventCount(const clap_input_events* pList);
    static const clap_event_header* CLAP_ABI eventAt(
            const clap_input_events* pList, uint32_t index);

    std::unique_ptr<CLAPPluginInstance> m_pInstance;
    const mixxx::audio::SampleRate m_sampleRate;
    QList<clap_id> m_parameterIds;
    // The value each parameter was last sent with.
    std::vector<double> m_sentValues;
    // Room for one change of every parameter per process call.
    std::vector<clap_event_param_value> m_events;
    uint32_t m_eventCount;
    // The first process call gives the plugin every value. A flag, not a
    // NaN in m_sentValues: Mixxx is built with fast floating point maths,
    // under which comparisons with NaN are not reliable.
    bool m_sendAllValues;
    std::vector<float> m_inputL;
    std::vector<float> m_inputR;
    std::vector<float> m_outputL;
    std::vector<float> m_outputR;
    // Every channel of the input ports Mixxx has no signal for reads this.
    std::vector<float> m_silence;
    // Every channel of the output ports Mixxx does not use is written here.
    std::vector<float> m_discarded;
    // One entry per port, as a process call takes them, and the channel
    // pointers those entries refer to.
    std::vector<clap_audio_buffer> m_inputPorts;
    std::vector<clap_audio_buffer> m_outputPorts;
    std::vector<std::vector<float*>> m_inputPortChannels;
    std::vector<std::vector<float*>> m_outputPortChannels;
};

class CLAPEffectProcessor final : public EffectProcessorImpl<CLAPEffectGroupState> {
  public:
    explicit CLAPEffectProcessor(CLAPManifestPointer pManifest);

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            CLAPEffectGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

    SINT getGroupDelayFrames() override {
        return m_groupDelayFrames.load(std::memory_order_relaxed);
    }

  private:
    CLAPEffectGroupState* createSpecificState(
            const mixxx::EngineParameters& engineParameters) override;

    CLAPManifestPointer m_pManifest;
    QList<EngineEffectParameterPointer> m_engineEffectParameters;
    // Written on the main thread when a state is created, read by the engine.
    std::atomic<SINT> m_groupDelayFrames;
};
