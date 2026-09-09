#include "waveform/renderers/waveformstemcolumns.h"

#include <algorithm>
#include <cmath>

#include "util/math.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace waveform {

void reduceStemColumns(const WaveformData* pData,
        int dataSize,
        int firstColumn,
        int columns,
        double framesPerColumn,
        StemColumnMax* pOut) {
    constexpr int kStems = StemColumnMax::kStems;
    const double halfRange = framesPerColumn / 2.0;
    double xVisualFrame = static_cast<double>(firstColumn) * framesPerColumn;
    for (int column = 0; column < columns; ++column) {
        StemColumnMax& out = pOut[column];
        std::fill(&out.value[0], &out.value[0] + kStems, uint8_t{0});

        const int visualFrameStart = static_cast<int>(std::lround(xVisualFrame - halfRange));
        const int visualFrameStop = static_cast<int>(std::lround(xVisualFrame + halfRange));
        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);

        // data is interleaved left / right; a stem's column value is the
        // maximum over both.
        for (int chn = 0; chn < 2; ++chn) {
            for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                const WaveformData& waveformData = pData[i];
                for (int stem = 0; stem < kStems; ++stem) {
                    out.value[stem] = math_max(out.value[stem], waveformData.stems[stem]);
                }
            }
        }
        xVisualFrame += framesPerColumn;
    }
}

} // namespace waveform
} // namespace mixxx
