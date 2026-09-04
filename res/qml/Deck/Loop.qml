import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Layouts
import QtQuick.Controls 2.12
import "../Theme"

Rectangle {
    id: root

    required property string group

    color: Theme.darkGray
    radius: 4

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 4
        color: Theme.lightGray3
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.pixelSize: Theme.buttonFontPixelSize
        text: qsTr("Loop")
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: loopEnabled

        group: root.group
        key: "loop_enabled"
    }
    BeatSizeSpinBoxBehavior {
        id: beatloopSize

        beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
        group: root.group
        key: "beatloop_size"
    }
    Mixxx.ControlProxy {
        id: loopHalve

        group: root.group
        key: "loop_halve"
    }
    Mixxx.ControlProxy {
        id: loopDouble

        group: root.group
        key: "loop_double"
    }
    RowLayout {
        anchors {
            left: parent.left
            leftMargin: 6
            right: parent.right
            rightMargin: 4
            top: parent.top
            topMargin: 22
        }

        Skin.ControlButton {
            id: loopInButton

            Layout.fillWidth: false
            Layout.minimumWidth: 28
            Layout.preferredWidth: 36
            activeColor: Theme.deckActiveColor
            enabled: trackLoadedControl.value > 0
            fontPixelSize: Theme.buttonFontPixelSize
            group: root.group
            implicitHeight: 26
            key: "loop_in"
            text: qsTr("In")
        }
        Skin.ControlButton {
            id: loopOutButton

            Layout.fillWidth: false
            Layout.minimumWidth: 28
            Layout.preferredWidth: 36
            activeColor: Theme.deckActiveColor
            enabled: trackLoadedControl.value > 0
            fontPixelSize: Theme.buttonFontPixelSize
            group: root.group
            implicitHeight: 26
            key: "loop_out"
            text: qsTr("Out")
        }
        Skin.ControlButton {
            id: loopRecallButton

            Layout.fillWidth: true
            Layout.minimumWidth: 56
            Layout.preferredWidth: 80
            activeColor: Theme.deckActiveColor
            enabled: trackLoadedControl.value > 0
            fontPixelSize: Theme.buttonFontPixelSize
            group: root.group
            implicitHeight: 26
            key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
            text: loopEnabled.value ? qsTr("Exit") : qsTr("Recall")
            toggleable: loopEnabled.value
        }
    }
    RowLayout {
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            onWheel: event => {
                if (event.angleDelta.y < 0) {
                    loopSizeRepeater.adjustSelectedIndex(-1);
                } else if (event.angleDelta.y > 0) {
                    loopSizeRepeater.adjustSelectedIndex(1);
                }
            }
        }

        anchors {
            bottom: parent.bottom
            bottomMargin: 6
            left: parent.left
            leftMargin: 6
            right: parent.right
            rightMargin: 6
        }

        Skin.Button {
            id: loopSizeHalfButton

            activeColor: Theme.deckActiveColor
            enabled: trackLoadedControl.value > 0
            implicitHeight: 28
            implicitWidth: 22
            glyph: Item {
                implicitHeight: 20
                implicitWidth: 20

                ChevronGlyph {
                    anchors.centerIn: parent
                    fillColor: loopSizeHalfButton.faceColor
                    forward: false
                }
            }

            onPressed: {
                loopSizeRepeater.adjustSelectedIndex(-1);
                if (loopEnabled.value) {
                    loopHalve.trigger();
                }
            }

            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onPressed: {
                    loopSizeRepeater.adjustSelectedIndex(-1);
                    if (!loopEnabled.value) {
                        loopHalve.trigger();
                    }
                }
            }
        }
        Repeater {
            id: loopSizeRepeater

            property int selectedIndex: 0
            property int valueCount: Math.min(Math.max(1, Math.floor((root.width - 61) / 38)), 4)
            property list<double> values: beatloopSize.beatSizes

            function adjustSelectedIndex(delta) {
                loopSizeRepeater.selectedIndex = Math.min(Math.max(0, loopSizeRepeater.selectedIndex + delta), loopSizeRepeater.values.length - 1);
            }
            function update() {
                let values = [this.values[selectedIndex]];
                let appendMode = values[0] <= this.values[0];
                while (values.length < valueCount) {
                    if (appendMode) {
                        values.push(values[values.length - 1] * 2);
                    } else {
                        values = [values[0] / 2, ...values];
                    }
                    if (values[0] == this.values[0]) {
                        appendMode = true;
                    } else if (values[values.length - 1] == this.values[this.values.length - 1]) {
                        appendMode = false;
                    } else {
                        appendMode = !appendMode;
                    }
                }
                model = values;
            }

            Component.onCompleted: {
                update();
                selectedIndex = values.indexOf(beatloopSize.value);
                if (selectedIndex < 0) {
                    selectedIndex = values.indexOf(4);
                }
            }
            onSelectedIndexChanged: update()
            onValueCountChanged: update()

            Connections {
                function onValueChanged() {
                    if (loopEnabled.value) {
                        parent.selectedIndex = parent.values.indexOf(beatloopSize.value);
                    }
                    parent.update();
                }

                target: beatloopSize
            }

            Skin.Button {
                id: loopSizeOpt1Button

                property double currentSize: modelData
                required property int index
                required property var modelData

                activeColor: Theme.deckActiveColor
                enabled: trackLoadedControl.value > 0
                highlight: currentSize == beatloopSize.value && trackLoadedControl.value > 0
                implicitHeight: 28
                implicitWidth: 33
                text: beatloopSize.formatBeatSize(currentSize)

                onPressed: {
                    if (loopEnabled.value) {
                        beatloopSize.commitText(currentSize.toString());
                    } else {
                        sizedBeatloopActivate.trigger();
                    }
                }

                Mixxx.ControlProxy {
                    id: sizedBeatloopActivate

                    group: root.group
                    key: `beatloop_${currentSize}_activate`
                }
            }
        }
        Skin.Button {
            id: loopSizeDoubleButton

            activeColor: Theme.deckActiveColor
            enabled: trackLoadedControl.value > 0
            implicitHeight: 28
            implicitWidth: 22
            glyph: Item {
                implicitHeight: 20
                implicitWidth: 20

                ChevronGlyph {
                    anchors.centerIn: parent
                    fillColor: loopSizeDoubleButton.faceColor
                    forward: true
                }
            }

            onPressed: {
                loopSizeRepeater.adjustSelectedIndex(1);
                if (loopEnabled.value) {
                    loopDouble.trigger();
                }
            }

            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onPressed: {
                    loopSizeRepeater.adjustSelectedIndex(1);
                    if (!loopEnabled.value) {
                        loopDouble.trigger();
                    }
                }
            }
        }
    }
}
