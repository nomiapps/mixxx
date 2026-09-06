#pragma once

#include <memory>

#include "rendergraph/attributeset.h"
#include "rendergraph/material.h"
#include "rendergraph/texture.h"
#include "rendergraph/uniformset.h"

namespace rendergraph {
class WaveformTexturedMaterial;
}

/// Material for the "high detail" waveform signal renderers: the whole
/// waveform is a texture (one texel per visual sample, low/mid/high/all in
/// the four channels) and the fragment shader looks up and draws the sample
/// under each pixel. One material class serves the three looks; the variant
/// only selects the fragment shader.
///
/// Uniform order matters twice over: it is the index passed to setUniform(),
/// and the scene graph backend copies the tightly packed uniform cache
/// straight into the std140 block, so the block is laid out mat4, vec4s,
/// vec2, floats, where tight packing and std140 agree.
class rendergraph::WaveformTexturedMaterial : public rendergraph::Material {
  public:
    enum class Variant {
        Filtered,
        RGB,
        Stacked,
    };

    enum Uniform {
        Matrix = 0,
        AxesColor,
        LowColor,
        MidColor,
        HighColor,
        LowFilteredColor,
        MidFilteredColor,
        HighFilteredColor,
        Size,
        AllGain,
        LowGain,
        MidGain,
        HighGain,
        FirstVisualIndex,
        LastVisualIndex,
        WaveformLength,
        TextureStride,
        SplitStereoSignal,
    };

    explicit WaveformTexturedMaterial(Variant variant);

    static const AttributeSet& attributes();
    static const UniformSet& uniforms();

    MaterialType* type() const override;
    std::unique_ptr<MaterialShader> createShader() const override;

    Texture* texture(int /*binding*/) const override {
        return m_pTexture.get();
    }
    void setTexture(std::unique_ptr<Texture> texture) {
        m_pTexture = std::move(texture);
    }

  private:
    const Variant m_variant;
    std::unique_ptr<Texture> m_pTexture;
};
