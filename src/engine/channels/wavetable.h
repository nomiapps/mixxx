#pragma once

#include <memory>
#include <vector>

#include "util/messagepipe.h"

/// A wavetable is a list of single-cycle frames the synth morphs between:
/// wt_position 0 plays the first frame, 1 the last, and anything between
/// crossfades the two frames either side of it. Frames are the Serum length,
/// so a Serum-format .wav (any multiple of 2048 sample frames) loads as is.
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
