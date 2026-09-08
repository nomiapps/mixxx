import Mixxx 1.0 as Mixxx
import QtQuick 2.12

/// Usually, this component shouldn't be an (visual) `Item` and use something
/// like `QtObject` instead. However, for some reason using `QtObject` here
/// makes Mixxx crash on load (using Qt 5.15.2+kde+r43-1). We can check if this
/// is fixed upstream once we switch to Qt 6.
Item {
    id: root

    required property string group
    property bool enabled: true

    signal loadTrackRequested(bool play)

    // No group, no controls. An Instantiator sets index to -1 on a delegate it
    // is tearing down, and the group binding built from that index resolved to
    // channel zero -- a group that does not exist -- so both proxies went
    // looking for it and logged a warning each on every teardown. The delegates
    // now pass an empty group in that moment; skip the lookup rather than make
    // it and complain.
    Loader {
        active: root.group.length > 0

        sourceComponent: QtObject {
            readonly property var loadProxy: Mixxx.ControlProxy {
                group: root.group
                key: "LoadSelectedTrack"

                onValueChanged: value => {
                    if (value == 0 || !root.enabled)
                        return;
                    root.loadTrackRequested(false);
                }
            }
            readonly property var loadAndPlayProxy: Mixxx.ControlProxy {
                group: root.group
                key: "LoadSelectedTrackAndPlay"

                onValueChanged: value => {
                    if (value == 0 || !root.enabled)
                        return;
                    root.loadTrackRequested(true);
                }
            }
        }
    }
}
