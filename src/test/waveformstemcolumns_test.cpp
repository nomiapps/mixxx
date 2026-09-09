#include "waveform/renderers/waveformstemcolumns.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "util/math.h"
#include "waveform/waveform.h"

namespace {

using mixxx::waveform::reduceStemColumns;
using mixxx::waveform::StemColumnMax;

// Tile width of WaveformRendererStemCached, repeated here rather than
// included: that class is scene-graph only and lives in the QML library,
// which this test binary is compiled against a different backend from.
constexpr int kTileColumns = 256;

constexpr int kStems = StemColumnMax::kStems;

// A deterministic waveform: interleaved stereo, values that vary per frame
// and differ per stem and channel so a wrong index shows up.
std::vector<WaveformData> makeWaveform(int frames) {
    std::vector<WaveformData> data(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames * 2; ++i) {
        WaveformData& d = data[i];
        d.filtered.low = 0;
        d.filtered.mid = 0;
        d.filtered.high = 0;
        d.filtered.all = 0;
        for (int stem = 0; stem < kStems; ++stem) {
            d.stems[stem] = static_cast<unsigned char>((i * (7 + stem * 6) + stem * 17) % 251);
        }
    }
    return data;
}

// The column loop of WaveformRendererStem::preprocessInner, verbatim in
// spirit, as the reference the cached renderer must reproduce.
void referenceReduce(const WaveformData* data,
        int dataSize,
        int firstColumn,
        int columns,
        double visualIncrementPerPixel,
        StemColumnMax* pOut) {
    double xVisualFrame = firstColumn * visualIncrementPerPixel;
    const double maxSamplingRange = visualIncrementPerPixel / 2.0;
    for (int pos = 0; pos < columns; ++pos) {
        const int visualFrameStart = std::lround(xVisualFrame - maxSamplingRange);
        const int visualFrameStop = std::lround(xVisualFrame + maxSamplingRange);
        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);
        for (int stemIdx = 0; stemIdx < kStems; ++stemIdx) {
            uchar u8max{};
            for (int chn = 0; chn < 2; chn++) {
                for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                    const WaveformData& waveformData = data[i];
                    u8max = math_max(u8max, waveformData.stems[stemIdx]);
                }
            }
            pOut[pos].value[stemIdx] = u8max;
        }
        xVisualFrame += visualIncrementPerPixel;
    }
}

TEST(WaveformStemColumnsTest, MatchesTheUncachedRendererAtSeveralZooms) {
    const int frames = 20000;
    const auto data = makeWaveform(frames);
    const int dataSize = static_cast<int>(data.size());
    for (double framesPerColumn : {0.37, 1.0, 2.5, 8.0, 33.3}) {
        const int columns = 700;
        for (int firstColumn : {0, 3, 517, 1000}) {
            std::vector<StemColumnMax> expected(columns);
            std::vector<StemColumnMax> actual(columns);
            referenceReduce(data.data(),
                    dataSize,
                    firstColumn,
                    columns,
                    framesPerColumn,
                    expected.data());
            reduceStemColumns(data.data(),
                    dataSize,
                    firstColumn,
                    columns,
                    framesPerColumn,
                    actual.data());
            for (int c = 0; c < columns; ++c) {
                for (int stem = 0; stem < kStems; ++stem) {
                    ASSERT_EQ(expected[c].value[stem], actual[c].value[stem])
                            << "fpc " << framesPerColumn << " first " << firstColumn
                            << " column " << c << " stem " << stem;
                }
            }
        }
    }
}

TEST(WaveformStemColumnsTest, ColumnsPastTheEndAreZero) {
    const auto data = makeWaveform(100);
    std::vector<StemColumnMax> out(50);
    reduceStemColumns(data.data(), static_cast<int>(data.size()), 90, 50, 2.0, out.data());
    for (int c = 10; c < 50; ++c) {
        for (int stem = 0; stem < kStems; ++stem) {
            EXPECT_EQ(0, out[c].value[stem]) << c;
        }
    }
}

TEST(WaveformStemColumnsTest, TilesAreOnTheSameGridAsAFullFrame) {
    // Reducing tile by tile must give the same columns as reducing the whole
    // visible range in one go, otherwise the cached tiles would not line up.
    const auto data = makeWaveform(50000);
    const int dataSize = static_cast<int>(data.size());
    const double fpc = 3.7;
    const int first = 1234;
    const int width = 1280;
    std::vector<StemColumnMax> whole(width);
    reduceStemColumns(data.data(), dataSize, first, width, fpc, whole.data());
    for (int tile = first / kTileColumns; tile <= (first + width - 1) / kTileColumns; ++tile) {
        std::vector<StemColumnMax> part(kTileColumns);
        reduceStemColumns(
                data.data(), dataSize, tile * kTileColumns, kTileColumns, fpc, part.data());
        for (int c = 0; c < kTileColumns; ++c) {
            const int global = tile * kTileColumns + c;
            if (global < first || global >= first + width) {
                continue;
            }
            for (int stem = 0; stem < kStems; ++stem) {
                ASSERT_EQ(whole[global - first].value[stem], part[c].value[stem]);
            }
        }
    }
}

// Not an assertion, a number: what one frame of the uncached renderer's
// column work costs at Edge width, which is what the cache removes from every
// frame after the first. The stem renderer draws a strip every other device
// pixel, so 2560 device pixels are 1280 columns.
TEST(WaveformStemColumnsTest, BenchmarkFullFrameReduce) {
    const auto data = makeWaveform(2000000);
    const int dataSize = static_cast<int>(data.size());
    const int width = 1280;
    std::vector<StemColumnMax> out(width);
    const int iterations = 50;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        reduceStemColumns(data.data(), dataSize, 100000 + i, width, 8.0, out.data());
    }
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start)
                                .count();
    // std::cout rather than qInfo: the test harness filters Qt logging.
    std::cout << "[ BENCH    ] reduce of " << width << " stem columns at 8 frames/column: "
              << (micros / iterations) << " us per frame per waveform" << std::endl;
    SUCCEED();
}

} // namespace
