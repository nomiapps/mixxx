#include "waveform/renderers/allshader/waveformrendererhighdetail.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "rendergraph/context.h"
#include "rendergraph/material/waveformtexturedmaterial.h"
#include "rendergraph/texture.h"
#include "rendergraph/vertexupdaters/texturedvertexupdater.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/logger.h"
#include "waveform/renderers/waveformtextureimage.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"

using namespace rendergraph;

namespace allshader {

namespace {

const mixxx::Logger kLogger("WaveformRendererHighDetail");

WaveformTexturedMaterial::Variant variantForType(WaveformWidgetType::Type type) {
    switch (type) {
    case WaveformWidgetType::RGB:
        return WaveformTexturedMaterial::Variant::RGB;
    case WaveformWidgetType::Stacked:
        return WaveformTexturedMaterial::Variant::Stacked;
    case WaveformWidgetType::Filtered:
        return WaveformTexturedMaterial::Variant::Filtered;
    default:
        DEBUG_ASSERT(!"unsupported WaveformWidgetType for the high detail renderer");
        return WaveformTexturedMaterial::Variant::Filtered;
    }
}

QVector4D colorVector(float r, float g, float b, float a = 1.f) {
    return QVector4D(r, g, b, a);
}

} // namespace

WaveformRendererHighDetail::WaveformRendererHighDetail(
        WaveformWidgetRenderer* waveformWidget,
        WaveformWidgetType::Type type,
        ::WaveformRendererAbstract::PositionSource positionSource,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_type(type),
          m_positionSource(positionSource),
          m_options(options) {
    setGeometry(std::make_unique<Geometry>(WaveformTexturedMaterial::attributes(), 0));
    setMaterial(std::make_unique<WaveformTexturedMaterial>(variantForType(type)));
    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    setUsePreprocess(true);
    kLogger.debug() << "created for" << waveformWidget->getGroup() << "type" << static_cast<int>(type);
}

void WaveformRendererHighDetail::onSetup(const QDomNode&) {
}

void WaveformRendererHighDetail::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

void WaveformRendererHighDetail::updateTextureIfNeeded(const Waveform* pWaveform) {
    // The completion grows while the analyzer runs; the uncached OpenGL
    // renderer re-uploads on every change too.
    const int completion = pWaveform->getCompletion();
    if (pWaveform == m_pTextureWaveform && completion == m_textureCompletion) {
        return;
    }
    Context* pContext = m_waveformRenderer->getContext();
    VERIFY_OR_DEBUG_ASSERT(pContext) {
        return;
    }
    auto pTexture = std::make_unique<Texture>(
            pContext, mixxx::waveform::waveformTextureImage(*pWaveform));
    // The shader samples texel centres and wants the raw bytes, not a blend
    // of neighbouring samples.
    pTexture->setNearestFiltering();
    static_cast<WaveformTexturedMaterial&>(material()).setTexture(std::move(pTexture));
    m_pTextureWaveform = pWaveform;
    m_textureCompletion = completion;
    ++m_textureUploads;
    markDirtyMaterial();
    if (completion >= pWaveform->getDataSize()) {
        // Once per track: the analyzer's partial uploads stay quiet.
        kLogger.debug() << "waveform texture uploaded for"
                        << m_waveformRenderer->getGroup() << "texels"
                        << pWaveform->getTextureSize() << "stride"
                        << pWaveform->getTextureStride();
    }
}

bool WaveformRendererHighDetail::preprocessInner() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();
    if (!pTrack) {
        return false;
    }
    if (m_positionSource == ::WaveformRendererAbstract::Slip &&
            !m_waveformRenderer->isSlipActive()) {
        return false;
    }

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull()) {
        return false;
    }
    const double audioVisualRatio = waveform->getAudioVisualRatio();
    const int dataSize = waveform->getDataSize();
    if (audioVisualRatio <= 0 || dataSize <= 1 || waveform->data() == nullptr) {
        return false;
    }
#ifdef __STEM__
    auto stemInfo = pTrack->getStemInfo();
    // A stem track is drawn by the stem renderer instead.
    if (!stemInfo.isEmpty() && waveform->hasStem() && !m_ignoreStem) {
        return false;
    }
#endif
    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return false;
    }
    if (!m_waveformRenderer->getContext()) {
        return false;
    }

    updateTextureIfNeeded(waveform.data());

    // One rectangle over the whole widget. Texture x spans the visible
    // range; texture y is 1 at the top (left channel), 0 at the bottom.
    const float length = static_cast<float>(m_waveformRenderer->getLength());
    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    geometry().allocate(6);
    TexturedVertexUpdater vertexUpdater{
            geometry().vertexDataAs<Geometry::TexturedPoint2D>()};
    vertexUpdater.addRectangle({0.f, 0.f}, {length, breadth}, {0.f, 1.f}, {1.f, 0.f});
    markDirtyGeometry();

    float allGain = 1.f;
    float lowGain = 1.f;
    float midGain = 1.f;
    float highGain = 1.f;
    getGains(&allGain, &lowGain, &midGain, &highGain);

    const auto firstVisualIndex = static_cast<float>(
            m_waveformRenderer->getFirstDisplayedPosition(m_positionSource) *
            trackSamples / audioVisualRatio / 2.0);
    const auto lastVisualIndex = static_cast<float>(
            m_waveformRenderer->getLastDisplayedPosition(m_positionSource) *
            trackSamples / audioVisualRatio / 2.0);

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    Material& mat = material();
    using U = WaveformTexturedMaterial::Uniform;
    mat.setUniform(U::AxesColor,
            colorVector(m_axesColor_r, m_axesColor_g, m_axesColor_b, m_axesColor_a));
    if (m_type == WaveformWidgetType::Filtered) {
        mat.setUniform(U::LowColor, colorVector(m_lowColor_r, m_lowColor_g, m_lowColor_b));
        mat.setUniform(U::MidColor, colorVector(m_midColor_r, m_midColor_g, m_midColor_b));
        mat.setUniform(U::HighColor, colorVector(m_highColor_r, m_highColor_g, m_highColor_b));
    } else {
        mat.setUniform(U::LowColor,
                colorVector(m_rgbLowColor_r, m_rgbLowColor_g, m_rgbLowColor_b));
        mat.setUniform(U::MidColor,
                colorVector(m_rgbMidColor_r, m_rgbMidColor_g, m_rgbMidColor_b));
        mat.setUniform(U::HighColor,
                colorVector(m_rgbHighColor_r, m_rgbHighColor_g, m_rgbHighColor_b));
    }
    mat.setUniform(U::LowFilteredColor,
            colorVector(m_rgbLowFilteredColor_r,
                    m_rgbLowFilteredColor_g,
                    m_rgbLowFilteredColor_b));
    mat.setUniform(U::MidFilteredColor,
            colorVector(m_rgbMidFilteredColor_r,
                    m_rgbMidFilteredColor_g,
                    m_rgbMidFilteredColor_b));
    mat.setUniform(U::HighFilteredColor,
            colorVector(m_rgbHighFilteredColor_r,
                    m_rgbHighFilteredColor_g,
                    m_rgbHighFilteredColor_b));
    mat.setUniform(U::Size, QVector2D(length * devicePixelRatio, breadth * devicePixelRatio));
    mat.setUniform(U::AllGain, allGain);
    mat.setUniform(U::LowGain, lowGain);
    mat.setUniform(U::MidGain, midGain);
    mat.setUniform(U::HighGain, highGain);
    mat.setUniform(U::FirstVisualIndex, firstVisualIndex);
    mat.setUniform(U::LastVisualIndex, lastVisualIndex);
    mat.setUniform(U::WaveformLength, static_cast<float>(dataSize));
    mat.setUniform(U::TextureStride, static_cast<float>(waveform->getTextureStride()));
    mat.setUniform(U::SplitStereoSignal,
            (m_options & ::WaveformRendererSignalBase::Option::SplitStereoSignal) ? 1.f : 0.f);
    markDirtyMaterial();

    return true;
}

} // namespace allshader
