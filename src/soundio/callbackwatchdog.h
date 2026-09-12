#pragma once

#include <QHash>
#include <QList>
#include <cstdint>

#include "soundio/soundmanagerutil.h"

/// Notices when an open sound device stops calling back.
///
/// A WASAPI stream can die under Mixxx without any error reaching it: the
/// PortAudio thread keeps waiting for an event the audio engine no longer
/// fires, the finished callback never runs, nothing is logged, and Mixxx sits
/// there silent with every deck stuck until the devices are reopened by hand
/// (Settings > Sound Hardware > Apply, or a restart). SoundManager feeds this
/// class one snapshot of the devices' callback counters per tick and reopens
/// whatever it reports.
///
/// Pure bookkeeping, no Qt object, no timer: that is what keeps it testable.
class CallbackWatchdog {
  public:
    struct Sample {
        SoundDeviceId id;
        uint64_t callbackCount;
    };
    struct Result {
        /// Devices whose counter has not moved for kStalledTicks ticks in a
        /// row. Each stall is reported once; the count starts over after it.
        QList<SoundDeviceId> stalled;
        /// Whether any device advanced since the previous tick.
        bool anyAdvanced = false;
        /// Devices seen for the first time since they opened, with the
        /// count they arrived with; SoundManager logs them once.
        QList<Sample> firstSeen;
    };

    /// Ticks without a new callback before a device counts as stalled. At
    /// SoundManager's 2 s tick that is 4 to 6 s of silence, long past any
    /// buffer size Mixxx offers and short enough that a set can go on.
    static constexpr int kStalledTicks = 3;

    /// One snapshot of every open device that reports callbacks. A device
    /// absent from a tick (closed meanwhile) is forgotten, so a reopen
    /// starts clean.
    Result tick(const QList<Sample>& samples);

    void reset() {
        m_seen.clear();
    }

  private:
    struct Seen {
        uint64_t count;
        int quietTicks;
    };
    QHash<SoundDeviceId, Seen> m_seen;
};
