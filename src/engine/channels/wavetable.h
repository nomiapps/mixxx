#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "util/messagepipe.h"

/// A wavetable is a list of single-cycle frames the synth morphs between:
/// wt_position 0 plays the first frame, 1 the last, and anything between
/// crossfades the two frames either side of it. Frames are the Serum length,
/// so a Serum-format .wav (any multiple of 2048 sample frames) loads as is.
///
/// A table can also be a grid: `columns` frames to a row, rows one after the
/// other. Then wt_position moves along a row and wt_position_y down the rows,
/// and a position between frames blends the four around it.
constexpr int kWavetableFrameSize = 2048;
/// Serum's own ceiling; 256 frames are 2.1 MB of floats.
constexpr int kWavetableMaxFrames = 256;
/// Every frame carries one extra sample, a copy of its first, so the
/// oscillator's linear interpolation can always read index i + 1 without
/// wrapping. Frame i of mip m starts at
/// samples[(m * frameCount + i) * kWavetableFrameStride].
constexpr int kWavetableFrameStride = kWavetableFrameSize + 1;
/// Band-limited copies of every frame: mip k keeps the partials up to
/// 1024 >> k (all 1024 at k = 0, four at k = 8), and the oscillator reads
/// the first mip whose partials all fit below Nyquist for its pitch, so a
/// bright frame does not alias in the top octaves. Nine levels reach
/// MIDI 105 alias-free; the octave and a half above that is left.
constexpr int kWavetableMipLevels = 9;
/// Partials mip k keeps.
constexpr int kWavetableMipPartials(int mip) {
    return (kWavetableFrameSize / 2) >> mip;
}

/// Plain PCM, no name or path: the engine never needs them, and keeping
/// QString out of memory the engine thread owns keeps the handoff trivial.
/// A table is immutable once built; the UI and the engine each hold their
/// own copy, so no reader ever needs a lock.
struct Wavetable {
    int frameCount = 0;
    /// Frames to a row when the table is a grid; 0 for a plain list. A value
    /// that does not divide frameCount into two rows or more is a plain list.
    int columns = 0;
    int mipCount = 1;           // 1 until wavetable::buildMips
    std::vector<float> samples; // mipCount * frameCount * kWavetableFrameStride

    /// Frame i of mip 0: the full-band frame, what the display draws.
    const float* frame(int i) const {
        return frame(i, 0);
    }
    float* frame(int i) {
        return frame(i, 0);
    }
    const float* frame(int i, int mip) const {
        return samples.data() +
                (static_cast<std::size_t>(mip) * frameCount + i) * kWavetableFrameStride;
    }
    float* frame(int i, int mip) {
        return samples.data() +
                (static_cast<std::size_t>(mip) * frameCount + i) * kWavetableFrameStride;
    }

    /// Frames to a row: all of them for a plain list.
    int gridColumns() const {
        return columns > 0 && columns < frameCount && frameCount % columns == 0
                ? columns
                : frameCount;
    }
    /// 1 for a plain list, 0 for an empty table.
    int gridRows() const {
        return frameCount > 0 ? frameCount / gridColumns() : 0;
    }

    /// Fills each frame's guard sample from its first sample, in every
    /// mip. Call after the samples are written and before the table is
    /// handed to anyone.
    void fillGuards() {
        for (int m = 0; m < mipCount; ++m) {
            for (int i = 0; i < frameCount; ++i) {
                frame(i, m)[kWavetableFrameSize] = frame(i, m)[0];
            }
        }
    }

    static std::unique_ptr<Wavetable> create(int frameCount, int mipCount = 1) {
        auto pTable = std::make_unique<Wavetable>();
        pTable->frameCount = frameCount;
        pTable->mipCount = mipCount;
        pTable->samples.assign(static_cast<std::size_t>(mipCount) * frameCount *
                        kWavetableFrameStride,
                0.0f);
        return pTable;
    }
};

/// Where a position reads in a table: the frames either side of x on the
/// rows either side of y, and how far between them. On a plain list, or on
/// the last row, both rows are the same and blendY is 0.
struct WavetableCell {
    int frameA0 = 0; // row A, left
    int frameB0 = 0; // row A, right
    int frameA1 = 0; // row B, left
    int frameB1 = 0; // row B, right
    double blendX = 0.0;
    double blendY = 0.0;

    /// x and y are 0..1 (clamped); y is ignored on a plain list.
    static WavetableCell locate(const Wavetable& table, double x, double y) {
        WavetableCell cell;
        const int columns = table.gridColumns();
        const int rows = table.gridRows();
        if (columns <= 0 || rows <= 0) {
            return cell;
        }
        const double column = std::clamp(x, 0.0, 1.0) * (columns - 1);
        const int left = std::min(static_cast<int>(column), columns - 1);
        const int right = std::min(left + 1, columns - 1);
        const double row = std::clamp(y, 0.0, 1.0) * (rows - 1);
        const int top = std::min(static_cast<int>(row), rows - 1);
        const int bottom = std::min(top + 1, rows - 1);
        cell.frameA0 = top * columns + left;
        cell.frameB0 = top * columns + right;
        cell.frameA1 = bottom * columns + left;
        cell.frameB1 = bottom * columns + right;
        cell.blendX = column - left;
        cell.blendY = rows > 1 ? row - top : 0.0;
        return cell;
    }
};

/// How a table reaches the engine: the effects-messenger idiom. The main
/// thread allocates a table and writes its raw pointer down the lane; the
/// engine adopts it at the top of its next process() and writes the pointer
/// it was using back up the lane, where the main thread deletes it. Nothing
/// is ever freed on the engine thread. The main side is a WavetableMainPipe,
/// the engine side a WavetableEnginePipe; makeTwoWayMessagePipe builds the
/// pair.
using WavetableMainPipe = MessagePipe<Wavetable*, Wavetable*>;
using WavetableEnginePipe = MessagePipe<Wavetable*, Wavetable*>;
/// Tables in flight in either direction before a write is refused.
constexpr int kWavetableLaneDepth = 8;
