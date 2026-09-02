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

    Skin.ControlSlider {
        anchors.fill: parent
        orientation: root.width > root.height ? Qt.Horizontal : Qt.Vertical
        group: root.groupResolved
        key: root.spec.key
        barStart: root.style === "volume" ? 0 : 0.5
        barColor: root.style === "tempo" ? Theme.bpmSliderBarColor : (root.style === "crossfader" ? Theme.crossfaderBarColor : Theme.volumeSliderBarColor)
        bg: root.style === "tempo" ? Theme.imgBpmSliderBackground : (root.style === "crossfader" ? Theme.imgCrossfaderBackground : Theme.imgVolumeSliderBackground)
        fg: root.style === "crossfader" ? Theme.imgCrossfaderHandle : Theme.imgSliderHandle
    }
}
