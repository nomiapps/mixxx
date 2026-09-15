#pragma once

#include <QList>
#include <QMultiHash>
#include <QString>
#include <QStringList>

#include "soundio/soundmanagerutil.h"

/// Puts the headphones where DJ hardware wires them.
///
/// A DJ controller with its own sound card presents four outputs: the main mix
/// on 1-2 and the headphone jack on 3-4. Mixxx's defaults only ever assign the
/// main output, so the controller plays through its master out and its
/// headphone jack stays silent until someone finds Settings > Sound Hardware
/// and routes Headphones to channels 3-4 by hand. Hardware should work as
/// hardware, so SoundManager asks this class once per device.
///
/// A device is considered when the main output is on its channels 1-2 and it
/// has at least four outputs. It is then remembered (the caller persists the
/// list), so a user who removes or moves the headphones afterwards is not
/// overruled. It is routed only if nothing is on Headphones anywhere yet and
/// channels 3-4 of that device are free.
///
/// Pure bookkeeping, no Qt object: that is what keeps it testable.
class HeadphoneRouting {
  public:
    struct Device {
        SoundDeviceId id;
        QString displayName;
        int outputChannels;
    };
    struct Result {
        bool routed = false;
        SoundDeviceId device;
        /// For the message telling the user where the headphones went.
        QString displayName;
    };

    /// Adds a Headphones output to pOutputs when a device qualifies, and
    /// appends every device it considered to pConsidered (keyed by device name).
    static Result apply(QMultiHash<SoundDeviceId, AudioOutput>* pOutputs,
            const QList<Device>& devices,
            QStringList* pConsidered);
};
