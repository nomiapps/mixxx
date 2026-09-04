import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property var spec
    property var surface: null
    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")

    // style presets: volume (bottom-zero bar), tempo (center-zero, bpm art),
    // crossfader (center-zero, crossfader art)
    readonly property string style: spec.style ?? "volume"
    readonly property bool horizontal: root.width > root.height

    // main: ControlSlider became ControlFader, and the bar settings are a
    // grouped BarSettings object (bar.start / bar.color) instead of barStart / barColor.
    Skin.ControlFader {
        anchors.fill: parent
        orientation: root.horizontal ? Qt.Horizontal : Qt.Vertical
        group: root.groupResolved
        key: root.spec.key
        bar.start: root.style === "volume" ? 0 : 0.5
        bar.color: root.style === "tempo" ? Theme.bpmSliderBarColor : (root.style === "crossfader" ? Theme.crossfaderBarColor : Theme.volumeSliderBarColor)
        bg: root.style === "tempo" ? Theme.imgBpmSliderBackground : (root.style === "crossfader" ? Theme.imgCrossfaderBackground : Theme.imgVolumeSliderBackground)
        fg: root.style === "crossfader" ? Theme.imgCrossfaderHandle : Theme.imgSliderHandle
    }

    // Shadow slot: a recessed line along the travel axis, dark core with a
    // faint highlight beside it, so the fader reads as seated in a groove.
    Rectangle {
        visible: !root.horizontal
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: 4
        height: parent.height - 10
        radius: 2
        color: "#000000"
        opacity: 0.4
    }

    Rectangle {
        visible: !root.horizontal
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: 3
        anchors.verticalCenter: parent.verticalCenter
        width: 1
        height: parent.height - 10
        color: Theme.pureWhite
        opacity: 0.07
    }

    Rectangle {
        visible: root.horizontal
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 10
        height: 4
        radius: 2
        color: "#000000"
        opacity: 0.4
    }

    Rectangle {
        visible: root.horizontal
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 3
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 10
        height: 1
        color: Theme.pureWhite
        opacity: 0.07
    }
}
