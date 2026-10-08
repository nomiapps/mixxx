import Edge.Controls 1.0 as EdgeControls
import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

EdgeControls.Fader {
    id: root

    property real backgroundMargin: bar.margin
    // Width of the slot artwork across the travel axis. Defaults to the whole
    // fader minus the margin, which is what anchors.fill gave before, so every
    // existing fader is unchanged. It exists for call sites that draw their own
    // groove: the slot SVG is stretched to whatever it is given, and a wide
    // stretched slot behind a narrow drawn one reads as two grooves. Those set
    // this to the width the art should actually occupy.
    property real backgroundWidth: Math.max(0, width - 2 * backgroundMargin)
    property alias bg: backgroundImage.source
    property alias fg: handleImage.source
    property alias handleImage: handleImage
    property bool showDefaultHandle: true
    // The cap no longer has a shadow (it was a shader effect). The property stays
    // so that callers which set it, upstream's LateNightQML skin among them,
    // still load.
    property bool showHandleShadow: true

    // How much to shrink the cap so it fits the fader across the travel axis.
    // The artwork is a fixed size -- 52x15 for slider_handle.svg, 15x52 for the
    // crossfader -- and the cap used to be drawn at exactly that size whatever
    // the fader measured, so a 24 px stem fader carried a 52 px cap, wider than
    // the control it belongs to. Only MixerColumn and Deck/HotcueAndStem worked
    // around it by hand; everywhere else was oversized.
    //
    // Never scales UP: the artwork's own size stays the maximum, so faders that
    // already had room look exactly as before. A call site that wants a smaller
    // cap still sets handleImage.width, and that simply becomes the size this
    // scales from, so the two existing overrides keep working unchanged.
    readonly property real capScale: {
        const natural = root.vertical ? handleImage.paintedWidth : handleImage.paintedHeight;
        const available = root.vertical ? root.width : root.height;
        if (natural <= 0 || available <= 0) {
            return 1;
        }
        return Math.min(1, available / natural);
    }

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

        anchors.centerIn: parent
        height: Math.max(0, root.height - 2 * root.backgroundMargin)
        width: root.backgroundWidth
    }
    handle: Item {
        id: handleItem

        height: handleImage.paintedHeight * root.capScale
        visible: root.showDefaultHandle
        width: handleImage.paintedWidth * root.capScale
        x: root.horizontal ? (root.visualPosition * (root.width - width)) : ((root.width - width) / 2)
        y: root.vertical ? (root.visualPosition * (root.height - height)) : ((root.height - height) / 2)

        RasterImage {
            id: handleSharp

            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            source: handleImage.source
        }
    }

    Image {
        id: handleImage

        fillMode: Image.PreserveAspectFit
        source: Theme.imgSliderHandle
        visible: false
    }
}
