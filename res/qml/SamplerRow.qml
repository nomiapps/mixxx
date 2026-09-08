pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts 1.12

GridLayout {
    id: root

    property int firstSampler: 1
    property int fxUnitCount: 4
    property int hotcueCount: 8
    property bool minimized: false
    property int samplerCount: 8
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showRateControl: true

    columnSpacing: 0
    columns: Math.max(1, root.samplerCount)
    rowSpacing: 0

    // main.qml raises [App],num_samplers to its final value on init, which is
    // after this row is first built, so asking PlayerManager for every strip up
    // front reaches samplers that do not exist yet: the delegate binds against a
    // null player and each of its control proxies resets with a warning. Size
    // the model to the count that actually exists and the row simply grows when
    // the rest arrive.
    Repeater {
        model: Math.min(Math.max(0, root.samplerCount), Math.max(0, numSamplersControl.value - (root.firstSampler - 1)))

        Skin.Sampler {
            required property int index

            Layout.fillWidth: true
            fxUnitCount: root.fxUnitCount
            group: "[Sampler" + (root.firstSampler + index) + "]"
            hotcueCount: root.hotcueCount
            minimized: root.minimized
            showFxAssignments: root.showFxAssignments
            showHotcues: root.showHotcues
            showRateControl: root.showRateControl
        }
    }
    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }
}
