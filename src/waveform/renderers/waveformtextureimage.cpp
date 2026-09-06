#include "waveform/renderers/waveformtextureimage.h"

#include <cstring>

#include "util/assert.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace waveform {

QImage packWaveformTexture(const WaveformData* pData, int dataSize, int stride, int textureSize) {
    VERIFY_OR_DEBUG_ASSERT(stride > 0 && textureSize >= stride && textureSize % stride == 0) {
        return QImage();
    }
    const int height = textureSize / stride;
    QImage image(stride, height, QImage::Format_RGBA8888_Premultiplied);
    image.fill(0);
    static_assert(sizeof(WaveformFilteredData) == 4, "one texel per element");
    const int count = std::min(std::max(dataSize, 0), textureSize);
    int i = 0;
    for (int row = 0; row < height && i < count; ++row) {
        uchar* pLine = image.scanLine(row);
        const int inRow = std::min(stride, count - i);
        for (int column = 0; column < inRow; ++column, ++i) {
            std::memcpy(pLine + column * 4, &pData[i].filtered, 4);
        }
    }
    return image;
}

QImage waveformTextureImage(const Waveform& waveform) {
    return packWaveformTexture(waveform.data(),
            waveform.getDataSize(),
            waveform.getTextureStride(),
            waveform.getTextureSize());
}

} // namespace waveform
} // namespace mixxx
