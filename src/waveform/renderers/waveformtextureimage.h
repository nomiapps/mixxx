#pragma once

#include <QImage>

class Waveform;
struct WaveformData;

namespace mixxx {
namespace waveform {

/// Packs the filtered bands of waveform data into an image the high detail
/// shaders sample: one texel per data element, (low, mid, high, all) in
/// (r, g, b, a), `stride` texels per row, `textureSize` texels in total
/// (Waveform guarantees a whole number of rows). Elements past dataSize are
/// zero.
///
/// The image is declared RGBA8888_Premultiplied although it holds raw data:
/// that is the one format both rendergraph backends upload byte for byte
/// without an alpha conversion that would scale the bands by "all".
QImage packWaveformTexture(const WaveformData* pData, int dataSize, int stride, int textureSize);

/// Convenience for a whole Waveform.
QImage waveformTextureImage(const Waveform& waveform);

} // namespace waveform
} // namespace mixxx
