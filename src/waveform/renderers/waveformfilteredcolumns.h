#pragma once

#include <cstdint>

struct WaveformData;

namespace mixxx {
namespace waveform {

/// Per-column maxima of the filtered waveform bands: [band][channel], band
/// 0 low, 1 mid, 2 high.
struct FilteredColumnMax {
    static constexpr int kBands = 3;
    uint8_t value[kBands][2];
};

/// The column reduction that WaveformRendererFiltered performs for every
/// pixel column of every frame, as a pure function so a renderer can cache
/// its result and a test can check and time it without a scene graph.
///
/// Column c on the grid covers visual frames
/// [c * framesPerColumn - half, c * framesPerColumn + half), sampled exactly
/// as WaveformRendererFiltered does. Columns past the end of the data come
/// back as zeros. Backend-neutral: no allshader or rendergraph types.
void reduceFilteredColumns(const WaveformData* pData,
        int dataSize,
        int firstColumn,
        int columns,
        double framesPerColumn,
        FilteredColumnMax* pOut);

} // namespace waveform
} // namespace mixxx
