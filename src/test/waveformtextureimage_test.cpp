#include "waveform/renderers/waveformtextureimage.h"

#include <gtest/gtest.h>

#include <vector>

#include "waveform/waveform.h"

namespace {

using mixxx::waveform::packWaveformTexture;

std::vector<WaveformData> makeData(int count) {
    std::vector<WaveformData> data(count);
    for (int i = 0; i < count; ++i) {
        data[i].filtered.low = static_cast<unsigned char>(i % 256);
        data[i].filtered.mid = static_cast<unsigned char>((i * 3) % 256);
        data[i].filtered.high = static_cast<unsigned char>((i * 7) % 256);
        data[i].filtered.all = static_cast<unsigned char>((i * 11) % 256);
    }
    return data;
}

TEST(WaveformTextureImageTest, PacksOneTexelPerElementRowMajor) {
    const int stride = 8;
    const int textureSize = 64;
    const auto data = makeData(50);
    const QImage image = packWaveformTexture(data.data(), 50, stride, textureSize);
    ASSERT_EQ(stride, image.width());
    ASSERT_EQ(textureSize / stride, image.height());
    ASSERT_EQ(QImage::Format_RGBA8888_Premultiplied, image.format());
    for (int i = 0; i < 50; ++i) {
        const uchar* pTexel = image.constScanLine(i / stride) + (i % stride) * 4;
        EXPECT_EQ(data[i].filtered.low, pTexel[0]) << i;
        EXPECT_EQ(data[i].filtered.mid, pTexel[1]) << i;
        EXPECT_EQ(data[i].filtered.high, pTexel[2]) << i;
        EXPECT_EQ(data[i].filtered.all, pTexel[3]) << i;
    }
}

TEST(WaveformTextureImageTest, ElementsPastDataSizeAreZero) {
    const auto data = makeData(64);
    const QImage image = packWaveformTexture(data.data(), 20, 8, 64);
    for (int i = 20; i < 64; ++i) {
        const uchar* pTexel = image.constScanLine(i / 8) + (i % 8) * 4;
        EXPECT_EQ(0, pTexel[0] | pTexel[1] | pTexel[2] | pTexel[3]) << i;
    }
}

TEST(WaveformTextureImageTest, RawBytesSurviveThePremultipliedLabel) {
    // A texel whose "all" byte is smaller than its bands would be scaled down
    // by a real premultiply. The packer must never do that.
    std::vector<WaveformData> data(1);
    data[0].filtered.low = 200;
    data[0].filtered.mid = 150;
    data[0].filtered.high = 100;
    data[0].filtered.all = 10;
    const QImage image = packWaveformTexture(data.data(), 1, 1, 1);
    const uchar* pTexel = image.constScanLine(0);
    EXPECT_EQ(200, pTexel[0]);
    EXPECT_EQ(150, pTexel[1]);
    EXPECT_EQ(100, pTexel[2]);
    EXPECT_EQ(10, pTexel[3]);
}

TEST(WaveformTextureImageTest, RejectsABadStride) {
    const auto data = makeData(8);
    EXPECT_TRUE(packWaveformTexture(data.data(), 8, 3, 8).isNull());
    EXPECT_TRUE(packWaveformTexture(data.data(), 8, 0, 8).isNull());
}

} // namespace
