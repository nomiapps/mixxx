#include "waveform/renderers/waveformfilteredcolumns.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "util/math.h"
#include "waveform/waveform.h"

namespace {

using mixxx::waveform::FilteredColumnMax;
using mixxx::waveform::reduceFilteredColumns;

// Tile width of WaveformRendererFilteredCached, repeated here rather than
// included: that class is scene-graph only and lives in the QML library,
// which this test binary is compiled against a different backend from.
constexpr int kTileColumns = 256;

// A deterministic waveform: interleaved stereo, values that vary per frame
// and differ per band and channel so a wrong index shows up.
std::vector<WaveformData> makeWaveform(int frames) {
    std::vector<WaveformData> data(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames * 2; ++i) {
        WaveformData& d = data[i];
        d.filtered.low = static_cast<unsigned char>((i * 7) % 251);
        d.filtered.mid = static_cast<unsigned char>((i * 13 + 5) % 253);
        d.filtered.high = static_cast<unsigned char>((i * 29 + 11) % 241);
        d.filtered.all = 0;
    }
    return data;
}

// The column loop of WaveformRendererFiltered::preprocessInner, verbatim in
// spirit, as the reference the cached renderer must reproduce.
void referenceReduce(const WaveformData* data,
        int dataSize,
        int firstColumn,
        int columns,
        double visualIncrementPerPixel,
        FilteredColumnMax* pOut) {
    double xVisualFrame = firstColumn * visualIncrementPerPixel;
    const double maxSamplingRange = visualIncrementPerPixel / 2.0;
    for (int pos = 0; pos < columns; ++pos) {
        const int visualFrameStart = std::lround(xVisualFrame - maxSamplingRange);
        const int visualFrameStop = std::lround(xVisualFrame + maxSamplingRange);
        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);
        uchar u8max[3][2]{};
        for (int chn = 0; chn < 2; chn++) {
            for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                const WaveformData& waveformData = data[i];
                u8max[0][chn] = math_max(u8max[0][chn], waveformData.filtered.low);
                u8max[1][chn] = math_max(u8max[1][chn], waveformData.filtered.mid);
                u8max[2][chn] = math_max(u8max[2][chn], waveformData.filtered.high);
            }
        }
        for (int band = 0; band < 3; ++band) {
            pOut[pos].value[band][0] = u8max[band][0];
            pOut[pos].value[band][1] = u8max[band][1];
        }
        xVisualFrame += visualIncrementPerPixel;
    }
}

TEST(WaveformFilteredColumnsTest, MatchesTheUncachedRendererAtSeveralZooms) {
    const int frames = 20000;
    const auto data = makeWaveform(frames);
    const int dataSize = static_cast<int>(data.size());
    for (double framesPerColumn : {0.37, 1.0, 2.5, 8.0, 33.3}) {
        const int columns = 700;
        for (int firstColumn : {0, 3, 517, 1000}) {
            std::vector<FilteredColumnMax> expected(columns);
            std::vector<FilteredColumnMax> actual(columns);
            referenceReduce(data.data(), dataSize, firstColumn, columns, framesPerColumn, expected.data());
            reduceFilteredColumns(data.data(), dataSize, firstColumn, columns, framesPerColumn, actual.data());
            for (int c = 0; c < columns; ++c) {
                for (int band = 0; band < 3; ++band) {
                    for (int chn = 0; chn < 2; ++chn) {
                        ASSERT_EQ(expected[c].value[band][chn], actual[c].value[band][chn])
                                << "fpc " << framesPerColumn << " first " << firstColumn
                                << " column " << c << " band " << band << " chn " << chn;
                    }
                }
            }
        }
    }
}

TEST(WaveformFilteredColumnsTest, ColumnsPastTheEndAreZero) {
    const auto data = makeWaveform(100);
    std::vector<FilteredColumnMax> out(50);
    reduceFilteredColumns(data.data(), static_cast<int>(data.size()), 90, 50, 2.0, out.data());
    for (int c = 10; c < 50; ++c) {
        for (int band = 0; band < 3; ++band) {
            EXPECT_EQ(0, out[c].value[band][0]) << c;
            EXPECT_EQ(0, out[c].value[band][1]) << c;
        }
    }
}

TEST(WaveformFilteredColumnsTest, TilesAreOnTheSameGridAsAFullFrame) {
    // Reducing tile by tile must give the same columns as reducing the whole
    // visible range in one go, otherwise the cached tiles would not line up.
    const auto data = makeWaveform(50000);
    const int dataSize = static_cast<int>(data.size());
    const double fpc = 3.7;
    const int first = 1234;
    const int width = 2560;
    std::vector<FilteredColumnMax> whole(width);
    reduceFilteredColumns(data.data(), dataSize, first, width, fpc, whole.data());
    for (int tile = first / kTileColumns; tile <= (first + width - 1) / kTileColumns; ++tile) {
        std::vector<FilteredColumnMax> part(kTileColumns);
        reduceFilteredColumns(data.data(), dataSize, tile * kTileColumns, kTileColumns, fpc, part.data());
        for (int c = 0; c < kTileColumns; ++c) {
            const int global = tile * kTileColumns + c;
            if (global < first || global >= first + width) {
                continue;
            }
            for (int band = 0; band < 3; ++band) {
                ASSERT_EQ(whole[global - first].value[band][0], part[c].value[band][0]);
                ASSERT_EQ(whole[global - first].value[band][1], part[c].value[band][1]);
            }
        }
    }
}

// Not an assertion, a number: what one frame of the uncached renderer's
// column work costs at Edge width, which is what the cache removes from
// every frame after the first.
TEST(WaveformFilteredColumnsTest, BenchmarkFullFrameReduce) {
    const auto data = makeWaveform(2000000);
    const int dataSize = static_cast<int>(data.size());
    const int width = 2560;
    std::vector<FilteredColumnMax> out(width);
    const int iterations = 50;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        reduceFilteredColumns(data.data(), dataSize, 100000 + i, width, 8.0, out.data());
    }
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start)
                                .count();
    // std::cout rather than qInfo: the test harness filters Qt logging.
    std::cout << "[ BENCH    ] reduce of " << width << " columns at 8 frames/column: "
              << (micros / iterations) << " us per frame per waveform" << std::endl;
    SUCCEED();
}

} // namespace
