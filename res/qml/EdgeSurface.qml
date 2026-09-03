import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import "Theme"

// The Edge performance surface: a layout ENGINE. Controller layouts are JSON
// files in res/qml/edge-layouts/ describing a design canvas plus elements
// (platter, pads, knob, slider, button, text) with rects and Mixxx
// group/key bindings. Add a .json there, list it in edge-layouts/index.json,
// and pick it from the LAYOUT menu; the canvas scales as one locked unit at
// any window size. (index.json exists because Qt.labs.folderlistmodel is not
// in the build; a C++ directory scan can replace it later.)
Window {
    id: root

    width: 2560
    height: 720
    color: Theme.backgroundColor
    title: "Mixxx - Edge Surface"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    opacity: 0

    property var layoutDef: null
    property bool placementReady: false

    function revealIfReady() {
        if (root.visible && root.placementReady && root.layoutDef)
            revealTimer.restart();
    }

    // The strip display this surface is built for: a screen whose physical
    // size is 2560x720 (any scaling), or failing that the widest-aspect
    // screen (>= 3:1). Null when no such display is attached.
    function edgeScreen() {
        const screens = Qt.application.screens;
        let best = null;
        for (let i = 0; i < screens.length; ++i) {
            const s = screens[i];
            const pw = Math.round(s.width * s.devicePixelRatio);
            const ph = Math.round(s.height * s.devicePixelRatio);
            if (pw === 2560 && ph === 720)
                return s;
            if (s.width / s.height >= 3 && (!best || s.width > best.width))
                best = s;
        }
        return best;
    }

    // Opening: put the surface ON the Edge, filling it, instead of wherever Qt
    // cascades a new window (the top of the main monitor). With no Edge
    // attached, clamp onto whatever screen we landed on so the title bar is
    // never offscreen. Deferred, because the window manager assigns the real
    // position after the visibility change.
    function placeOnScreen() {
        const edge = edgeScreen();
        if (edge) {
            root.screen = edge;
            root.x = edge.virtualX;
            root.y = edge.virtualY;
            root.width = edge.width;
            root.height = edge.height;
            root.raise();
            root.placementReady = true;
            root.revealIfReady();
            return ;
        }
        const s = root.screen;
        if (!s) {
            root.opacity = 1;
            return ;
        }

        root.width = Math.min(root.width, s.desktopAvailableWidth);
        root.height = Math.min(root.height, s.desktopAvailableHeight);
        root.x = Math.min(Math.max(root.x, s.virtualX),
                s.virtualX + s.desktopAvailableWidth - root.width);
        root.y = Math.min(Math.max(root.y, s.virtualY),
                s.virtualY + s.desktopAvailableHeight - root.height);
        root.raise();
        root.placementReady = true;
        root.revealIfReady();
    }

    onVisibleChanged: {
        if (visible) {
            root.opacity = 0;
            root.placementReady = false;
            clampTimer.start();
        } else {
            root.opacity = 0;
        }
    }

    Timer {
        id: clampTimer

        interval: 250
        onTriggered: root.placeOnScreen()
    }

    Timer {
        id: revealTimer

        interval: 0
        onTriggered: {
            root.raise();
            root.opacity = 1;
        }
    }

    Connections {
        target: root.transientParent
        ignoreUnknownSignals: true

        function onActiveChanged() {
            if (root.visible && root.transientParent && root.transientParent.active)
                root.raise();
        }
    }

    // Deck slots: layouts may write @left / @right anywhere a group appears
    // (including inside rack groups like [EqualizerRack1_@left_Effect1]), and
    // a 'deckswitch' element retargets a slot at runtime - hardware-style
    // deck select. Reassigning the object notifies every bound element.
    property var deckAssign: ({
            "@left": "[Channel1]",
            "@right": "[Channel2]"
        })

    function resolveGroup(group) {
        if (!group)
            return "";

        let resolved = group;
        for (const slot in deckAssign)
            resolved = resolved.split(slot).join(deckAssign[slot]);
        return resolved;
    }

    function setDeck(slot, group) {
        const next = Object.assign({}, deckAssign);
        next[slot] = group;
        deckAssign = next;
    }

    function elementFile(type) {
        switch (type) {
        case "platter":
            return "EdgeElementPlatter.qml";
        case "pads":
            return "EdgeElementPads.qml";
        case "knob":
            return "EdgeElementKnob.qml";
        case "slider":
            return "EdgeElementSlider.qml";
        case "button":
            return "EdgeElementButton.qml";
        case "text":
            return "EdgeElementText.qml";
        case "label":
            return "EdgeElementLabel.qml";
        case "vumeter":
            return "EdgeElementVuMeter.qml";
        case "waveform":
            return "EdgeElementWaveform.qml";
        case "overview":
            return "EdgeElementOverview.qml";
        case "deckswitch":
            return "EdgeElementDeckSwitch.qml";
        case "divider":
            return "EdgeElementDivider.qml";
        default:
            console.warn("edge-layout: unknown element type", type);
            return "";
        }
    }

    function loadLayout(url) {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = () => {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return ;

            try {
                root.layoutDef = JSON.parse(xhr.responseText);
                root.revealIfReady();
            } catch (e) {
                console.warn("edge-layout: failed to parse", url, e);
            }
        };
        xhr.open("GET", url);
        xhr.send();
    }

    ListModel {
        id: layoutList
    }

    Component.onCompleted: {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = () => {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return ;

            try {
                const index = JSON.parse(xhr.responseText);
                for (const file of index.layouts) {
                    layoutList.append({
                        "name": file.replace(/\.json$/, ""),
                        "url": Qt.resolvedUrl("edge-layouts/" + file).toString()
                    });
                }
                if (layoutList.count > 0) {
                    layoutPicker.currentIndex = 0;
                    root.loadLayout(layoutList.get(0).url);
                }
            } catch (e) {
                console.warn("edge-layout: failed to read index.json", e);
            }
        };
        xhr.open("GET", Qt.resolvedUrl("edge-layouts/index.json"));
        xhr.send();
    }

    Rectangle {
        id: header

        width: parent.width
        height: 32
        color: Theme.toolbarBackgroundColor

        Row {
            anchors.verticalCenter: parent.verticalCenter
            x: 8
            spacing: 10

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "LAYOUT"
                color: Theme.deckTextColor
                font.pixelSize: 12
            }

            // Opaque themed dropdown. Skin.ComboBox uses the deck "embedded"
            // (semi-transparent) background, which reads as see-through for a
            // floating toolbar popup, so this one is styled opaque locally.
            ComboBox {
                id: layoutPicker

                width: 280
                height: 26
                model: layoutList
                textRole: "name"
                onActivated: (index) => {
                    root.loadLayout(layoutList.get(index).url);
                }

                background: Rectangle {
                    color: Theme.knobBackgroundColor
                    radius: 4
                    border.color: layoutPicker.pressed ? Theme.blue : Theme.midGray
                    border.width: 1
                }

                contentItem: Text {
                    leftPadding: 8
                    rightPadding: layoutPicker.indicator.width + 4
                    text: layoutPicker.displayText
                    color: Theme.deckTextColor
                    font.pixelSize: 12
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                delegate: ItemDelegate {
                    id: layoutItem

                    required property int index
                    width: layoutPicker.width
                    highlighted: layoutPicker.highlightedIndex === index

                    contentItem: Text {
                        text: layoutPicker.textAt(layoutItem.index)
                        color: Theme.deckTextColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        color: layoutItem.highlighted ? Qt.rgba(0.004, 0.863, 0.988, 0.18) : "transparent"
                    }
                }

                popup: Popup {
                    y: layoutPicker.height + 2
                    width: layoutPicker.width
                    implicitHeight: Math.min(contentItem.implicitHeight, 400)
                    padding: 4

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: layoutPicker.popup.visible ? layoutPicker.delegateModel : null
                        currentIndex: layoutPicker.highlightedIndex
                        ScrollIndicator.vertical: ScrollIndicator {}
                    }
                    background: Rectangle {
                        color: Theme.darkGray2
                        radius: 4
                        border.color: Theme.midGray
                        border.width: 1
                    }
                }
            }
        }
    }

    Item {
        id: canvasArea

        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        readonly property real canvasW: root.layoutDef ? root.layoutDef.canvas[0] : 2560
        readonly property real canvasH: root.layoutDef ? root.layoutDef.canvas[1] : 720
        readonly property real ui: Math.min(width / canvasW, height / canvasH)
        readonly property real xOff: (width - canvasW * ui) / 2
        readonly property real yOff: (height - canvasH * ui) / 2
        readonly property real dpr: root.screen ? root.screen.devicePixelRatio : 1

        function pixelAligned(value) {
            return Math.round(value * dpr) / dpr;
        }

        Repeater {
            model: root.layoutDef ? root.layoutDef.elements : []

            Loader {
                required property var modelData

                x: canvasArea.pixelAligned(canvasArea.xOff + modelData.rect[0] * canvasArea.ui)
                y: canvasArea.pixelAligned(canvasArea.yOff + modelData.rect[1] * canvasArea.ui)
                width: canvasArea.pixelAligned(canvasArea.xOff + (modelData.rect[0] + modelData.rect[2]) * canvasArea.ui) - x
                height: canvasArea.pixelAligned(canvasArea.yOff + (modelData.rect[1] + modelData.rect[3]) * canvasArea.ui) - y
                Component.onCompleted: {
                    const file = root.elementFile(modelData.type);
                    if (file)
                        setSource(file, {
                            "spec": modelData,
                            "surface": root
                        });
                }
            }
        }
    }
}
