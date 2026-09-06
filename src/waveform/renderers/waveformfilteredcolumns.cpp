#include "waveform/renderers/waveformfilteredcolumns.h"

#include <algorithm>
#include <cmath>

#include "util/math.h"
#include "waveform/waveform.h"

namespace mixxx {
namespace waveform {

void reduceFilteredColumns(const WaveformData* pData,
        int dataSize,
        int firstColumn,
        int columns,
        double framesPerColumn,
        FilteredColumnMax* pOut) {
    constexpr int kBands = FilteredColumnMax::kBands;
    const double halfRange = framesPerColumn / 2.0;
    double xVisualFrame = static_cast<double>(firstColumn) * framesPerColumn;
    for (int column = 0; column < columns; ++column) {
        FilteredColumnMax& out = pOut[column];
        std::fill(&out.value[0][0], &out.value[0][0] + kBands * 2, uint8_t{0});

        const int visualFrameStart = static_cast<int>(std::lround(xVisualFrame - halfRange));
        const int visualFrameStop = static_cast<int>(std::lround(xVisualFrame + halfRange));
        const int visualIndexStart = std::max(visualFrameStart * 2, 0);
        const int visualIndexStop =
                std::min(std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);

        for (int chn = 0; chn < 2; ++chn) {
            uint8_t low = 0;
            uint8_t mid = 0;
            uint8_t high = 0;
            for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                const WaveformData& waveformData = pData[i];
                low = math_max(low, waveformData.filtered.low);
                mid = math_max(mid, waveformData.filtered.mid);
                high = math_max(high, waveformData.filtered.high);
            }
            out.value[0][chn] = low;
            out.value[1][chn] = mid;
            out.value[2][chn] = high;
        }
        xVisualFrame += framesPerColumn;
    }
}

} // namespace waveform
} // namespace mixxx
