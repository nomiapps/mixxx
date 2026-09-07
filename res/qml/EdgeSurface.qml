import QtQuick 2.12
import QtQuick.Window 2.12
import "Theme"

// The Edge performance surface: a layout ENGINE. Controller layouts are JSON
// files in res/qml/edge-layouts/ describing a design canvas plus elements
// (platter, pads, knob, slider, button, text, synth) with rects and Mixxx
// group/key bindings. Add a .json there, list it in edge-layouts/index.json,
// and pick it from the LAYOUT menu; the canvas scales as one locked unit at
// any window size. (index.json exists because Qt.labs.folderlistmodel is not
// in the build; a C++ directory scan can replace it later.)
Window {
    id: root

    // Deck slots: layouts may write @left / @right anywhere a group appears
    // (including inside rack groups like [EqualizerRack1_@left_Effect1]), and
    // a 'deckswitch' element retargets a slot at runtime - hardware-style
    // deck select. Reassigning the object notifies every bound element.
    property var deckAssign: ({
            "@left": "[Channel1]",
            "@right": "[Channel2]"
        })
    property var layoutDef: null
    // True when a real strip display was found. It decides the window's chrome:
    // on the strip we are a frameless, always-on-top, focus-refusing panel bolted
    // to the hardware; anywhere else (a Surface Pro, a laptop) that is hostile --
    // an undraggable, unresizable window you cannot dismiss -- so we become an
    // ordinary window instead. Settled before the window is placed, because
    // changing flags re-creates the native window.
    property bool onStrip: false
    property bool placementReady: false
    // Layout-local view flags flipped by 'surfacetoggle' elements. Nothing here
    // reaches the engine: a flag only decides which elements exist and where.
    // Seeded from the layout's top-level "toggles" object; unnamed flags are off.
    property var toggles: ({})

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
    // An element with "showIf" exists only while that flag is on. Loader.active,
    // not visible: a hidden waveform should stop rendering, not draw off-screen.
    function elementActive(el) {
        const flag = el.showIf ?? "";
        return flag === "" || root.toggleOn(flag);
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
        case "library":
            return "EdgeElementLibrary.qml";
        case "surfacetoggle":
            return "EdgeElementSurfaceToggle.qml";
        case "overview":
            return "EdgeElementOverview.qml";
        case "deckswitch":
            return "EdgeElementDeckSwitch.qml";
        case "divider":
            return "EdgeElementDivider.qml";
        case "synth":
            return "EdgeElementSynth.qml";
        default:
            console.warn("edge-layout: unknown element type", type);
            return "";
        }
    }

    // "rectIf": {"<flag>": [x, y, w, h]} -- the rect to use while that flag is on,
    // so switching a section on can reflow the element it displaces. First match
    // wins; with no match the element keeps its own rect.
    function elementRect(el) {
        const alt = el.rectIf ?? {};
        for (const flag in alt) {
            if (root.toggleOn(flag))
                return alt[flag];
        }
        return el.rect;
    }

    // No strip attached, so derive the window from the display we are on rather
    // than from the hardware we are imitating. Everything below is in logical
    // pixels -- Screen.width already has the panel's scaling applied, so a
    // 2880x1920 Surface Pro at 200% reports 1440x960 and needs no DPI maths of
    // our own. Keep the layout canvas's aspect so the controls stay at their
    // designed proportions, take 90% of the smaller constraint, and centre it.
    function fitToScreen(s) {
        const availW = s.desktopAvailableWidth;
        const availH = s.desktopAvailableHeight;
        const canvasW = root.layoutDef ? root.layoutDef.canvas[0] : 2560;
        const canvasH = root.layoutDef ? root.layoutDef.canvas[1] : 720;
        const aspect = canvasW / canvasH;
        let w = Math.round(availW * 0.9);
        let h = Math.round(w / aspect) + header.height;
        if (h > availH * 0.9) {
            h = Math.round(availH * 0.9);
            w = Math.round((h - header.height) * aspect);
        }
        // A canvas taller than the desktop (a portrait screen, a stacked layout)
        // can drive the width negative; fall back to filling what we have.
        if (w < 320 || h < 240) {
            w = Math.min(Math.round(availW * 0.9), availW);
            h = Math.min(Math.round(availH * 0.9), availH);
        }
        root.width = w;
        root.height = h;
        root.x = s.virtualX + Math.round((availW - w) / 2);
        root.y = s.virtualY + Math.round((availH - h) / 2);
    }
    function loadLayout(url) {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = () => {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return;

            try {
                const def = root.resolveThemeColors(JSON.parse(xhr.responseText));
                root.toggles = Object.assign({}, def.toggles ?? {});
                root.layoutDef = def;
                root.revealIfReady();
            } catch (e) {
                console.warn("edge-layout: failed to parse", url, e);
            }
        };
        xhr.open("GET", url);
        xhr.send();
    }

    // Opening: put the surface ON the Edge, filling it, instead of wherever Qt
    // cascades a new window (the top of the main monitor). With no Edge
    // attached, size the window from the screen we are actually on instead of
    // the strip's 2560x720, which is wider than a Surface Pro's whole desktop.
    // Deferred, because the window manager assigns the real position after the
    // visibility change.
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
            return;
        }
        const s = root.screen ?? Qt.application.screens[0];
        if (!s) {
            root.opacity = 1;
            return;
        }
        root.fitToScreen(s);
        root.raise();
        root.placementReady = true;
        root.revealIfReady();
    }
    function resolveGroup(group) {
        if (!group)
            return "";

        let resolved = group;
        for (const slot in deckAssign)
            resolved = resolved.split(slot).join(deckAssign[slot]);
        return resolved;
    }

    // An element's "color" may name a Theme property ("amber", "red") instead of
    // carrying a hex literal, so a layout inherits the palette rather than pinning it.
    // Literals still work -- anything starting with '#' is passed through untouched.
    function resolveThemeColors(def) {
        for (const el of (def.elements ?? [])) {
            if (typeof el.color === "string" && !el.color.startsWith("#")) {
                const resolved = Theme[el.color];
                if (resolved !== undefined)
                    el.color = resolved;
                else
                    console.warn("edge-layout: no Theme colour named", el.color);
            }
        }
        return def;
    }
    function revealIfReady() {
        if (root.visible && root.placementReady && root.layoutDef)
            revealTimer.restart();
    }
    function setDeck(slot, group) {
        const next = Object.assign({}, deckAssign);
        next[slot] = group;
        deckAssign = next;
    }
    function setToggle(flag, on) {
        const next = Object.assign({}, toggles);
        next[flag] = on === true;
        toggles = next;
    }
    function toggleOn(flag) {
        return flag !== "" && root.toggles[flag] === true;
    }

    color: Theme.backgroundColor
    flags: root.onStrip ? (Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus) : Qt.Window
    height: 720
    // Anything is resizable once it is an ordinary window, and canvasArea
    // rescales the layout to whatever it is given, so no minimum beyond legibility.
    minimumHeight: root.onStrip ? 0 : 240
    minimumWidth: root.onStrip ? 0 : 320
    opacity: 0
    title: "Mixxx - Edge Surface"
    width: 2560

    Component.onCompleted: {
        const xhr = new XMLHttpRequest();
        xhr.onreadystatechange = () => {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return;

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
    onVisibleChanged: {
        if (visible) {
            root.opacity = 0;
            root.placementReady = false;
            // Decide the chrome first: this re-creates the native window, and it
            // has to happen before clampTimer positions it, or the flag change
            // discards the geometry we just set. We are still at opacity 0, so
            // the re-creation is not visible.
            root.onStrip = !!root.edgeScreen();
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
        function onActiveChanged() {
            if (root.visible && root.transientParent && root.transientParent.active)
                root.raise();
        }

        ignoreUnknownSignals: true
        target: root.transientParent
    }
    ListModel {
        id: layoutList
    }
    // The layout picker. Off the strip it is an ordinary toolbar and takes its own
    // height. ON the strip it must not: the canvas scales as one locked unit, so 32px
    // taken off a 720px panel scaled the WHOLE surface to 95.6% and left a 57px black
    // bar down each side -- the layout was never using the display it was built for.
    // There it floats over the canvas instead and stays invisible until the pointer
    // reaches the top edge. Hidden by opacity, not visible: a zero-opacity item still
    // receives hover, which is what brings it back.
    Rectangle {
        id: header

        color: Theme.toolbarBackgroundColor
        height: 32
        opacity: (!root.onStrip || headerHover.hovered) ? 1 : 0
        width: parent.width
        z: 1

        Behavior on opacity {
            NumberAnimation {
                duration: 120
            }
        }

        HoverHandler {
            id: headerHover
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            x: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.deckTextColor
                font.pixelSize: 12
                text: "LAYOUT"
            }
            OpaqueComboBox {
                id: layoutPicker

                font.pixelSize: 12
                height: 26
                model: layoutList
                popupMaxHeight: 400
                popupWidth: width
                textRole: "name"
                width: 280

                onActivated: index => {
                    root.loadLayout(layoutList.get(index).url);
                }
            }
        }
    }
    Item {
        id: canvasArea

        readonly property real canvasH: root.layoutDef ? root.layoutDef.canvas[1] : 720
        readonly property real canvasW: root.layoutDef ? root.layoutDef.canvas[0] : 2560
        readonly property real dpr: root.screen ? root.screen.devicePixelRatio : 1
        readonly property real ui: Math.min(width / canvasW, height / canvasH)
        readonly property real xOff: (width - canvasW * ui) / 2
        readonly property real yOff: (height - canvasH * ui) / 2

        function pixelAligned(value) {
            return Math.round(value * dpr) / dpr;
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: root.onStrip ? parent.top : header.bottom

        Repeater {
            model: root.layoutDef ? root.layoutDef.elements : []

            Loader {
                id: elementLoader

                // The rect in force right now, which "rectIf" may swap out. Reading it
                // through the surface is what re-lays the element when a flag changes.
                readonly property var box: root.elementRect(modelData)
                required property var modelData

                function loadElement() {
                    const file = root.elementFile(modelData.type);
                    if (file)
                        setSource(file, {
                            "spec": modelData,
                            "surface": root
                        });
                }

                active: root.elementActive(modelData)
                height: canvasArea.pixelAligned(canvasArea.yOff + (box[1] + box[3]) * canvasArea.ui) - y
                width: canvasArea.pixelAligned(canvasArea.xOff + (box[0] + box[2]) * canvasArea.ui) - x
                x: canvasArea.pixelAligned(canvasArea.xOff + box[0] * canvasArea.ui)
                y: canvasArea.pixelAligned(canvasArea.yOff + box[1] * canvasArea.ui)

                Component.onCompleted: elementLoader.loadElement()
                // An element that starts switched off has no source yet; give it one the
                // first time it is switched on. After that the Loader reloads it itself.
                onActiveChanged: {
                    if (elementLoader.active && elementLoader.source == "")
                        elementLoader.loadElement();
                }
            }
        }
    }
}
