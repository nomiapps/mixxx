import "." as Skin
import QtQuick 2.12
import "Theme"

Item {
    id: root

    readonly property string groupResolved: surface ? surface.resolveGroup(spec.group ?? "") : (spec.group ?? "")
    readonly property bool horizontal: root.width > root.height
    required property var spec

    // style presets: volume (bottom-zero bar), tempo (center-zero, bpm art),
    // crossfader (center-zero, crossfader art)
    readonly property string style: spec.style ?? "volume"
    property var surface: null

    // main: ControlSlider became ControlFader, and the bar settings are a
    // grouped BarSettings object (bar.start / bar.color) instead of barStart / barColor.
    Skin.ControlFader {
        anchors.fill: parent
        bar.color: root.style === "tempo" ? Theme.bpmSliderBarColor : (root.style === "crossfader" ? Theme.crossfaderBarColor : Theme.volumeSliderBarColor)
        bar.start: root.style === "volume" ? 0 : 0.5
        bg: root.style === "tempo" ? Theme.imgBpmSliderBackground : (root.style === "crossfader" ? Theme.imgCrossfaderBackground : Theme.imgVolumeSliderBackground)
        fg: root.style === "crossfader" ? Theme.imgCrossfaderHandle : Theme.imgSliderHandle
        group: root.groupResolved
        key: root.spec.key
        orientation: root.horizontal ? Qt.Horizontal : Qt.Vertical
    }

    // Shadow slot: a recessed line along the travel axis, dark core with a
    // faint highlight beside it, so the fader reads as seated in a groove.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: "#000000"
        height: parent.height - 10
        opacity: 0.4
        radius: 2
        visible: !root.horizontal
        width: 4
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: 3
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.pureWhite
        height: parent.height - 10
        opacity: 0.07
        visible: !root.horizontal
        width: 1
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        color: "#000000"
        height: 4
        opacity: 0.4
        radius: 2
        visible: root.horizontal
        width: parent.width - 10
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 3
        color: Theme.pureWhite
        height: 1
        opacity: 0.07
        visible: root.horizontal
        width: parent.width - 10
    }
}
