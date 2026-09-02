import "." as Skin
import QtQuick 2.12

Item {
    id: root

    required property var spec

    Skin.WaveformDisplay {
        anchors.fill: parent
        group: root.spec.group
        splitStemTracks: root.spec.splitStems === true
    }
}
