#include "soundio/callbackwatchdog.h"

CallbackWatchdog::Result CallbackWatchdog::tick(const QList<Sample>& samples) {
    Result result;
    QHash<SoundDeviceId, Seen> next;
    for (const Sample& sample : samples) {
        Seen seen{sample.callbackCount, 0};
        const auto it = m_seen.constFind(sample.id);
        if (it == m_seen.constEnd()) {
            // First sight of this device since it opened: nothing to compare
            // with yet. A device that opens and never calls back at all is
            // caught on the following ticks, its counter still at zero.
            next.insert(sample.id, seen);
            result.firstSeen.append(sample);
            continue;
        }
        if (sample.callbackCount != it->count) {
            result.anyAdvanced = true;
        } else {
            seen.quietTicks = it->quietTicks + 1;
            if (seen.quietTicks >= kStalledTicks) {
                result.stalled.append(sample.id);
                seen.quietTicks = 0;
            }
        }
        next.insert(sample.id, seen);
    }
    m_seen = next;
    return result;
}
