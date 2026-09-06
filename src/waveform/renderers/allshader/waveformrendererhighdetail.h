#pragma once

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/widgets/waveformwidgettype.h"

class Waveform;

namespace allshader {
class WaveformRendererHighDetail;
} // namespace allshader

/// The "high detail" waveform signal on rendergraph, so it works on both the
/// OpenGL and the Qt Quick scene graph backends (mixxxdj/mixxx#14990).
///
/// WaveformRendererTextured does the same job with raw QOpenGL: the whole
/// waveform is uploaded as a texture and a fragment shader draws, for every
/// pixel, the sample under it, which is what makes the look sharper than the
/// per-column geometry renderers at high zoom. This is that design expressed
/// as one textured rectangle with a WaveformTexturedMaterial: the texture is
/// rebuilt when the track or the analysis progress changes, and each frame
/// only updates the visible range, gains and colours in the uniform block.
///
/// Differences from the OpenGL renderer: no 4x oversampled framebuffer pass
/// (it draws at device resolution), and no slip position source yet.
class allshader::WaveformRendererHighDetail final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererHighDetail(WaveformWidgetRenderer* waveformWidget,
            WaveformWidgetType::Type type,
            ::WaveformRendererAbstract::PositionSource positionSource =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);
    ~WaveformRendererHighDetail() override = default;

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    // Virtual for rendergraph::Node
    void preprocess() override;

    /// Diagnostics, also for tests: how many times the waveform texture has
    /// been (re)uploaded.
    int textureUploads() const {
        return m_textureUploads;
    }

  private:
    bool preprocessInner();
    void updateTextureIfNeeded(const Waveform* pWaveform);

    const WaveformWidgetType::Type m_type;
    const ::WaveformRendererAbstract::PositionSource m_positionSource;
    const ::WaveformRendererSignalBase::Options m_options;
    const Waveform* m_pTextureWaveform{nullptr};
    int m_textureCompletion{-1};
    int m_textureUploads{0};

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererHighDetail);
};
