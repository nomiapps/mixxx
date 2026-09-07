import QtQuick
import QtQuick.Window

// An Image that rasterises its SVG at device resolution without re-rasterising
// on every pixel of a resize.
//
// The obvious way to get a crisp SVG is `sourceSize: size * devicePixelRatio`.
// That is correct and it is what the knobs, faders and the Edge platter did,
// but sourceSize is part of the image's cache key and these images load
// synchronously, so every single size change decodes the SVG again on the GUI
// thread. Measured with qml.exe: 3.6 ms for one such image per size change, and
// 14.9 ms for a cluster of 24 knobs (48 images) -- paid on every step of a
// window resize, which is why resizing the window dragged.
//
// So round the raster size up to a bucket. The image is then rasterised at up
// to one bucket larger than it is drawn and scaled down, which is the direction
// that looks good, and it is re-decoded only when the size crosses a bucket
// edge: 1.7 ms for the same 24 knobs, an 8.7x reduction.
Image {
    id: root

    // Decode off the GUI thread. Measured: a fresh rasterisation of the fader
    // art costs 1-3 ms synchronously and 0 ms asynchronously, and Qt keeps
    // showing the previous pixmap until the new one is ready, so a resize does
    // not flash. The cost is one frame of staleness on the very first load,
    // where there is no previous pixmap to keep.
    asynchronous: true
    // Bucket in device pixels. 32 keeps the worst-case oversample modest for
    // controls of this size while making a drag-resize cross a bucket rarely.
    property int rasterBucket: 32

    function bucketed(logical) {
        return Math.max(root.rasterBucket, Math.ceil(logical * Screen.devicePixelRatio / root.rasterBucket) * root.rasterBucket);
    }

    // Both dimensions bucket independently, which is what the callers were
    // already doing by setting both. It cannot distort a PreserveAspectFit
    // image (sourceSize is only a bounding box there), and the one Stretch
    // caller -- the fader background -- is stretched by design.
    sourceSize.height: root.bucketed(root.height)
    sourceSize.width: root.bucketed(root.width)
}
