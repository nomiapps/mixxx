#include "waveformtexturedmaterial.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "rendergraph/materialshader.h"
#include "rendergraph/materialtype.h"
#include "rendergraph/uniformset.h"

using namespace rendergraph;

WaveformTexturedMaterial::WaveformTexturedMaterial(Variant variant)
        : Material(uniforms()),
          m_variant(variant) {
}

/* static */ const AttributeSet& WaveformTexturedMaterial::attributes() {
    static AttributeSet set = makeAttributeSet<QVector2D, QVector2D>({"position", "texcoord"});
    return set;
}

/* static */ const UniformSet& WaveformTexturedMaterial::uniforms() {
    static UniformSet set = makeUniformSet<QMatrix4x4,
            QVector4D,
            QVector4D,
            QVector4D,
            QVector4D,
            QVector4D,
            QVector4D,
            QVector4D,
            QVector2D,
            float,
            float,
            float,
            float,
            float,
            float,
            float,
            float,
            float>({"ubuf.matrix",
            "ubuf.axesColor",
            "ubuf.lowColor",
            "ubuf.midColor",
            "ubuf.highColor",
            "ubuf.lowFilteredColor",
            "ubuf.midFilteredColor",
            "ubuf.highFilteredColor",
            "ubuf.size",
            "ubuf.allGain",
            "ubuf.lowGain",
            "ubuf.midGain",
            "ubuf.highGain",
            "ubuf.firstVisualIndex",
            "ubuf.lastVisualIndex",
            "ubuf.waveformLength",
            "ubuf.textureStride",
            "ubuf.splitStereoSignal"});
    return set;
}

MaterialType* WaveformTexturedMaterial::type() const {
    // One shader program per variant, so one type per variant.
    static MaterialType filteredType;
    static MaterialType rgbType;
    static MaterialType stackedType;
    switch (m_variant) {
    case Variant::RGB:
        return &rgbType;
    case Variant::Stacked:
        return &stackedType;
    case Variant::Filtered:
    default:
        return &filteredType;
    }
}

std::unique_ptr<MaterialShader> WaveformTexturedMaterial::createShader() const {
    const char* fragmentShader = "waveformtexturedfiltered.frag";
    switch (m_variant) {
    case Variant::RGB:
        fragmentShader = "waveformtexturedrgb.frag";
        break;
    case Variant::Stacked:
        fragmentShader = "waveformtexturedstacked.frag";
        break;
    case Variant::Filtered:
    default:
        break;
    }
    return std::make_unique<MaterialShader>(
            "waveformtextured.vert", fragmentShader, uniforms(), attributes());
}
