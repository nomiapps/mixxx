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

            ComboBox {
                id: layoutPicker

                width: 280
                height: 26
                model: layoutList
                textRole: "name"
                onActivated: (index) => {
                    root.loadLayout(layoutList.get(index).url);
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
                            "spec": modelData
                        });
                }
            }
        }
    }
}
