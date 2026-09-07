import Edge.Controls 1.0 as EdgeControls
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

EdgeControls.Fader {
    id: root

    property real backgroundMargin: bar.margin
    property alias bg: backgroundImage.source
    property alias fg: handleImage.source
    property alias handleImage: handleImage
    property bool showDefaultHandle: true
    property bool showHandleShadow: true

    bar.enabled: true
    bar.margin: 10
    // Size comes from a hidden probe at the source's NATURAL size, not from the
    // visible image. Setting sourceSize on the visible one (so the SVG
    // rasterises at device resolution instead of being upscaled on a
    // fractional-DPR screen) also changes its implicitWidth/Height, which fed
    // straight back into the fader's own size. The probe keeps the two apart.
    implicitHeight: naturalSize.implicitHeight
    implicitWidth: naturalSize.implicitWidth

    Image {
        id: naturalSize

        source: backgroundImage.source
        visible: false
    }
    // RasterImage buckets the raster size. This one fills the fader, so with a
    // plain sourceSize binding it re-decoded its SVG on every pixel of a window
    // resize; the fader art is stretched by design, so bucketing costs nothing.
    background: RasterImage {
        id: backgroundImage

        anchors.fill: parent
        anchors.margins: root.backgroundMargin
    }
    handle: Item {
        id: handleItem

        height: handleImage.paintedHeight
        visible: root.showDefaultHandle
        width: handleImage.paintedWidth
        x: root.horizontal ? (root.visualPosition * (root.width - width)) : ((root.width - width) / 2)
        y: root.vertical ? (root.visualPosition * (root.height - height)) : ((root.height - height) / 2)

        RasterImage {
            id: handleSharp

            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            source: handleImage.source
            visible: !root.showHandleShadow
        }
        DropShadow {
            color: "#80000000"
            height: parent.height + 5
            radius: 5
            // Shadow the DPR-rasterised copy, not handleImage: that one is the
            // natural-size probe the handle takes its dimensions from, and as a
            // texture it is the SVG at 1x, upscaled on a fractional-DPR screen.
            // handleSharp is hidden whenever this shadow is shown, so it serves
            // as the source without drawing twice.
            source: handleSharp
            verticalOffset: 5
            visible: root.showHandleShadow
            width: parent.width + 5
        }
    }

    Image {
        id: handleImage

        fillMode: Image.PreserveAspectFit
        source: Theme.imgSliderHandle
        visible: false
    }
}
