pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts 1.12
import "Theme"

RowLayout {
    id: root

    // Which bank of strips is on screen, from 0. With fewer strips than
    // samplers the row pages through them; it is not remembered, so a restart
    // opens on sampler 1.
    property int bank: 0
    readonly property int bankCount: Math.max(1, Math.ceil(root.samplersAvailable / root.samplerCount))
    readonly property int firstSampler: root.bank * root.samplerCount + 1
    property int fxUnitCount: 4
    property int hotcueCount: 8
    property bool minimized: false
    // How many strips share the width: Settings > Interface > Samplers shown.
    // Fewer strips make each one wider, which is what a touch screen needs.
    property int samplerCount: [2, 4, 8].includes(Mixxx.Config.samplerStripsShown) ? Mixxx.Config.samplerStripsShown : 8
    readonly property int samplersAvailable: Math.max(0, numSamplersControl.value)
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showLength: true
    property bool showRateControl: true
    property bool touchSize: Mixxx.Config.samplerTouchSize

    spacing: 0

    // Fewer banks after the count went up: stay on the last one that exists
    // rather than showing an empty row.
    onBankCountChanged: root.bank = Math.min(root.bank, root.bankCount - 1)

    ColumnLayout {
        readonly property int columnWidth: root.touchSize ? 44 : 30

        Layout.fillHeight: true
        // Pinned: the row hands out spare width to whatever will take it, and
        // with two wide strips that made the arrows a quarter of the screen.
        Layout.maximumWidth: columnWidth
        Layout.minimumWidth: columnWidth
        Layout.preferredWidth: columnWidth
        spacing: 2
        visible: root.bankCount > 1

        Skin.Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            enabled: root.bank > 0
            fontPixelSize: 24
            implicitWidth: 0
            opacity: enabled ? 1 : 0.4
            text: "‹"

            onClicked: root.bank = root.bank - 1
        }
        // The sampler numbers on screen, first over last.
        Text {
            Layout.fillWidth: true
            color: Theme.deckTextColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.buttonFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            text: root.firstSampler + "\n" + Math.min(root.samplersAvailable, root.firstSampler + root.samplerCount - 1)
        }
        Skin.Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            enabled: root.bank < root.bankCount - 1
            fontPixelSize: 24
            implicitWidth: 0
            opacity: enabled ? 1 : 0.4
            text: "›"

            onClicked: root.bank = root.bank + 1
        }
    }
    GridLayout {
        Layout.fillWidth: true
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
            model: Math.min(Math.max(0, root.samplerCount), Math.max(0, root.samplersAvailable - (root.firstSampler - 1)))

            Skin.Sampler {
                required property int index

                Layout.fillWidth: true
                fxUnitCount: root.fxUnitCount
                group: "[Sampler" + (root.firstSampler + index) + "]"
                hotcueCount: root.hotcueCount
                minimized: root.minimized
                showFxAssignments: root.showFxAssignments
                showHotcues: root.showHotcues
                showLength: root.showLength
                showRateControl: root.showRateControl
                touchSize: root.touchSize
            }
        }
    }
    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }
}
