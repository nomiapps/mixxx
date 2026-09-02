import "." as Skin
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

    property var layoutDef: null

    // Same clamp as main.qml: never open with the title bar offscreen.
    // Deferred, because the window manager assigns the real position after
    // the visibility change.
    function clampOntoScreen() {
        const s = root.screen;
        if (!s)
            return ;

        root.width = Math.min(root.width, s.desktopAvailableWidth);
        root.height = Math.min(root.height, s.desktopAvailableHeight);
        root.x = Math.min(Math.max(root.x, s.virtualX),
                s.virtualX + s.desktopAvailableWidth - root.width);
        root.y = Math.min(Math.max(root.y, s.virtualY),
                s.virtualY + s.desktopAvailableHeight - root.height);
    }

    onVisibleChanged: {
        if (visible)
            clampTimer.start();
    }

    Timer {
        id: clampTimer

        interval: 250
        onTriggered: root.clampOntoScreen()
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

        Repeater {
            model: root.layoutDef ? root.layoutDef.elements : []

            Loader {
                required property var modelData

                x: canvasArea.xOff + modelData.rect[0] * canvasArea.ui
                y: canvasArea.yOff + modelData.rect[1] * canvasArea.ui
                width: modelData.rect[2] * canvasArea.ui
                height: modelData.rect[3] * canvasArea.ui
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
