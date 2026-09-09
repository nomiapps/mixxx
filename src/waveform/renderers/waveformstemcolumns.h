#pragma once

#include <cstdint>

#include "engine/engine.h"

struct WaveformData;

namespace mixxx {
namespace waveform {

/// Per-column maximum of each stem's waveform data.
struct StemColumnMax {
    static constexpr int kStems = mixxx::kMaxSupportedStems;
    uint8_t value[kStems];
};

/// The column reduction that WaveformRendererStem performs for every strip of
/// every frame, as a pure function so a renderer can cache its result and a
/// test can check it without a scene graph.
///
/// Column c on the grid covers visual frames
/// [c * framesPerColumn - half, c * framesPerColumn + half), sampled exactly
/// as WaveformRendererStem does -- including its one departure from the
/// filtered renderer: a stem's value is the maximum over BOTH channels, not
/// one value per channel, because a stem lane is drawn as a single symmetric
/// shape rather than a split left/right pair. Columns past the end of the
/// data come back as zeros. Backend-neutral: no allshader or rendergraph
/// types.
void reduceStemColumns(const WaveformData* pData,
        int dataSize,
        int firstColumn,
        int columns,
        double framesPerColumn,
        StemColumnMax* pOut);

} // namespace waveform
} // namespace mixxx
