import "." as Skin
import Mixxx 1.0 as Mixxx
import Edge.Controls 1.0 as EdgeControls
import QtQuick 2.12

// Which side of the crossfader this channel plays on: left, both, or right.
//
// It is a three-position control, but it used to be drawn as a bare hairline
// with a tick on it and no chrome of any kind, so in a row of buttons it read
// as a slider someone had failed to finish -- and in a narrow strip, like the
// sampler's, the line ran under its neighbours' labels. It now sits in the
// same frame as the buttons beside it and shows all three positions, with the
// current one filled.
Item {
    id: root

    property color color: "white"
    required property string group
    required property string key
    property alias orientation: orientationSlider.value

    implicitHeight: 26
    implicitWidth: 56

    // The same chrome the buttons in these rows use, so the control belongs to
    // the row instead of floating in it.
    Rectangle {
        anchors.fill: parent
        border.color: "#343740"
        border.width: 1
        color: "#17181b"
        radius: 4
    }
    Skin.Fader {
        id: orientationSlider

        anchors.bottomMargin: 4
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        clip: true
        from: 0
        live: false
        orientation: Qt.Horizontal
        snapMode: EdgeControls.Slider.SnapOnRelease
        stepSize: 1
        to: 2
        value: control.value
        wheelEnabled: false

        background: Item {
            id: sliderBackground

            anchors.fill: parent

            // Three seats, drawn dim. The filled marker below sits on whichever
            // one is selected, so the control shows its range at rest instead
            // of only where it happens to be.
            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    color: root.color
                    height: 8
                    opacity: 0.35
                    radius: 1
                    width: 2
                    // Positioned rather than anchored: a Repeater's items are
                    // parented after creation, so `parent` is briefly null and
                    // anchoring to it throws on every one of them.
                    x: index * (sliderBackground.width - width) / 2
                    y: (sliderBackground.height - height) / 2
                }
            }
        }
        handle: Rectangle {
            id: indicator

            color: root.color
            height: 14
            radius: 1
            width: 4
            x: orientationSlider.visualPosition * (sliderBackground.width - width)
            y: (orientationSlider.height - height) / 2

            Behavior on x {
                NumberAnimation {
                    duration: 150
                }
            }
        }

        onMoved: function (value) {
            if (value != control.value)
                control.value = value;
        }
    }
    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }
}
