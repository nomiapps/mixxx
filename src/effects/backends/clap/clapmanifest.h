#pragma once

#include <clap/clap.h>

#include <QByteArray>
#include <QList>
#include <QSharedPointer>

#include "effects/backends/clap/clapplugin.h"
#include "effects/backends/effectmanifest.h"

/// Describes one plugin of a CLAPLibrary: its name and the parameters Mixxx
/// offers as knobs and buttons.
class CLAPManifest : public EffectManifest {
  public:
    enum class Status {
        Available,
        NotInstantiable,
        IoNotStereo,
    };

    /// Creates the plugin once, without activating it, to read its audio
    /// ports and parameters.
    CLAPManifest(CLAPLibraryPointer pLibrary, const clap_plugin_descriptor* pDescriptor);

    bool isValid() const {
        return m_status == Status::Available;
    }
    Status status() const {
        return m_status;
    }

    const CLAPLibraryPointer& library() const {
        return m_pLibrary;
    }
    /// The plugin's id as the plugin factory expects it.
    const QByteArray& pluginId() const {
        return m_pluginId;
    }
    /// The CLAP parameter ids, in the order of parameters().
    const QList<clap_id>& parameterIds() const {
        return m_parameterIds;
    }
    /// A parameter Mixxx sets once and does not offer as a control.
    struct FixedParameter {
        clap_id id;
        double value;
    };
    /// The plugin's own dry/wet mix, held fully wet: the effect unit's mix
    /// knob does that job, and could otherwise only reach the plugin's share.
    const QList<FixedParameter>& fixedParameters() const {
        return m_fixedParameters;
    }
    /// The channel count of every audio input port. The first is the stereo
    /// port Mixxx plays through; any others, such as a sidechain, get silence.
    const QList<uint32_t>& inputPortChannels() const {
        return m_inputPortChannels;
    }
    /// The channel count of every audio output port. Mixxx takes the first
    /// and ignores what the plugin writes to the others.
    const QList<uint32_t>& outputPortChannels() const {
        return m_outputPortChannels;
    }

  private:
    void readParameters(const CLAPPluginInstance& instance);

    CLAPLibraryPointer m_pLibrary;
    QByteArray m_pluginId;
    QList<clap_id> m_parameterIds;
    QList<FixedParameter> m_fixedParameters;
    QList<uint32_t> m_inputPortChannels;
    QList<uint32_t> m_outputPortChannels;
    Status m_status;
};

typedef QSharedPointer<CLAPManifest> CLAPManifestPointer;
