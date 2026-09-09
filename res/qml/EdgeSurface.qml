import Mixxx 1.0 as Mixxx
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
        case "sequencer":
            return "EdgeElementSequencer.qml";
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

    // Reflow. A layout authored for the strip is a ROW of sections separated by
    // vertical seams -- x ranges that no element's rect spans. On a screen that
    // is not strip-shaped those sections can be stacked instead of laid end to
    // end, which lets the whole layout scale up instead of sitting in a
    // letterbox: on a 3:2 Surface Pro the Mixtrack layout goes from filling 44%
    // of the screen to 63%, and vertical-waveforms from 44% to 97%.
    //
    // Nothing is distorted and nothing is cut. The scale stays uniform, only the
    // arrangement changes, and cutting only at a seam means no element can
    // straddle a row boundary. On the Edge the single row always scales largest,
    // so the strip keeps exactly the geometry it has today.
    readonly property var bands: computeBands()
    readonly property real bandGutter: 12
    // Two element rects closer than this are one section: a seam has to be a
    // real visual gap, not the hairline between neighbours.
    readonly property real seamGap: 8

    // Which band a canvas x falls in. Bands are built from element extents, so
    // every element's left edge lies inside one.
    function bandAt(plan, cx) {
        const bs = plan.bands;
        for (let i = 0; i < bs.length; ++i) {
            if (cx <= bs[i][1] + root.seamGap)
                return i;
        }
        return bs.length - 1;
    }

    // Try every way of cutting the bands into contiguous rows and keep the one
    // that scales largest. That is 2^(n-1) candidates and n is a handful today
    // (7 at most); the cap is there so a future layout cannot make resizing
    // exponential without anyone noticing.
    function buildPlan(availW, availH) {
        const bs = root.bands;
        const canvasH = root.layoutDef ? root.layoutDef.canvas[1] : 720;
        const n = Math.min(bs.length, 12);
        const bandWidth = i => bs[i][1] - bs[i][0];
        let best = null;
        for (let mask = 0; mask < (1 << (n - 1)); ++mask) {
            const rows = [];
            let row = [0];
            for (let i = 0; i < n - 1; ++i) {
                if ((mask >> i) & 1) {
                    rows.push(row);
                    row = [];
                }
                row.push(i + 1);
            }
            rows.push(row);
            let totalW = 0;
            for (const r of rows) {
                let w = root.bandGutter * (r.length - 1);
                for (const b of r)
                    w += bandWidth(b);
                totalW = Math.max(totalW, w);
            }
            const totalH = rows.length * canvasH + root.bandGutter * (rows.length - 1);
            const scale = Math.min(availW / totalW, availH / totalH);
            if (!best || scale > best.scale)
                best = {
                    "scale": scale,
                    "rows": rows,
                    "totalW": totalW,
                    "totalH": totalH
                };
        }
        // Turn the winning cut into a per-band offset in canvas units: dx slides
        // a band along its row (rows are centred against the widest one), dy
        // drops it to its row.
        const place = [];
        for (let r = 0; r < best.rows.length; ++r) {
            const rowBands = best.rows[r];
            let w = root.bandGutter * (rowBands.length - 1);
            for (const b of rowBands)
                w += bandWidth(b);
            let cursor = (best.totalW - w) / 2;
            for (const b of rowBands) {
                place[b] = {
                    "dx": cursor - bs[b][0],
                    "dy": r * (canvasH + root.bandGutter)
                };
                cursor += bandWidth(b) + root.bandGutter;
            }
        }
        best.place = place;
        best.bands = bs;
        return best;
    }

    // The seams, found from every rect an element can occupy -- including its
    // "rectIf" alternatives, so flipping a toggle cannot re-cut the layout
    // underneath and shuffle everything sideways.
    function computeBands() {
        const canvasW = root.layoutDef ? root.layoutDef.canvas[0] : 2560;
        const els = root.layoutDef ? (root.layoutDef.elements ?? []) : [];
        const spans = [];
        for (const el of els) {
            const cands = [el.rect];
            const alt = el.rectIf ?? {};
            for (const flag in alt)
                cands.push(alt[flag]);
            for (const r of cands) {
                if (r && r.length === 4)
                    spans.push([r[0], r[0] + r[2]]);
            }
        }
        if (spans.length === 0)
            return [[0, canvasW]];

        spans.sort((a, b) => a[0] - b[0]);
        const out = [];
        let start = spans[0][0];
        let cursor = spans[0][1];
        for (let i = 1; i < spans.length; ++i) {
            if (spans[i][0] > cursor + root.seamGap) {
                out.push([start, cursor]);
                start = spans[i][0];
            }
            cursor = Math.max(cursor, spans[i][1]);
        }
        out.push([start, cursor]);
        return out;
    }

    // No strip attached, so derive the window from the display we are on rather
    // than from the hardware we are imitating. Everything below is in logical
    // pixels -- Screen.width already has the panel's scaling applied, so a
    // 2880x1920 Surface Pro at 200% reports 1440x960 and needs no DPI maths of
    // our own. Keep the layout canvas's aspect so the controls stay at their
    // designed proportions, and centre it on that screen.
    //
    // Measure the SCREEN, never Screen.desktopAvailableWidth/Height: those report
    // the whole VIRTUAL DESKTOP and read identically from every screen object, so
    // with a second display attached this sized the window to the combined
    // desktop. Measured on a 3-monitor setup, all three screens report
    // desktopAvailable 5120x2880, which asked for a 4608px-wide window on a 2560px
    // screen and left Windows to clamp it to whatever it could fit. Qt exposes no
    // per-screen work area to QML, so the 0.9 is what keeps us clear of the taskbar.
    function fitToScreen(s) {
        const availW = s.width * 0.9;
        const availH = s.height * 0.9;
        // Size to the REFLOWED shape, not to the canvas. The window has to be the
        // shape the layout will actually take on this screen, or the reflow is
        // handed back the strip aspect it was trying to escape and picks a single
        // row every time. buildPlan also contains the fit, so a canvas taller than
        // the screen can no longer drive the width negative.
        const plan = root.buildPlan(availW, availH - header.height);
        let w = Math.round(plan.totalW * plan.scale);
        let h = Math.round(plan.totalH * plan.scale) + header.height;
        // A screen too short to hold the header and a legible canvas drives the
        // scale to zero or below; fall back to filling what we have.
        if (w < 320 || h < 240) {
            w = Math.round(availW);
            h = Math.round(availH);
        }
        root.width = w;
        root.height = h;
        root.x = s.virtualX + Math.round((s.width - w) / 2);
        root.y = s.virtualY + Math.round((s.height - h) / 2);
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
                // Layouts do not all reflow to the same shape, so the window that
                // fitted the last one is the wrong size for this one. Re-fit off
                // the strip only; on the strip the window IS the panel and must
                // not move.
                if (!root.onStrip && root.placementReady && root.screen)
                    root.fitToScreen(root.screen);
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
    title: "Mixxx - Edge Surface · build " + Mixxx.Application.buildTag
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
            LinkButton {
                anchors.verticalCenter: parent.verticalCenter
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

        readonly property real dpr: root.screen ? root.screen.devicePixelRatio : 1
        readonly property var plan: root.buildPlan(Math.max(1, width), Math.max(1, height))
        readonly property real ui: plan.scale
        readonly property real xOff: (width - plan.totalW * ui) / 2
        readonly property real yOff: (height - plan.totalH * ui) / 2

        // Canvas coordinates to surface coordinates, through the band the point
        // belongs to. With a single row every offset is zero and this is the plain
        // scale-and-centre it has always been.
        function mapX(cx) {
            return xOff + (cx + plan.place[root.bandAt(plan, cx)].dx) * ui;
        }
        // y needs the element's x as well: which row it lands in is decided by
        // which band it is in.
        function mapY(cy, cx) {
            return yOff + (cy + plan.place[root.bandAt(plan, cx)].dy) * ui;
        }
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
                // Size from the mapped origin plus the scaled extent rather than
                // mapping the far corner: that corner can sit on a seam, where
                // which band it belongs to is ambiguous.
                height: canvasArea.pixelAligned(canvasArea.mapY(box[1], box[0]) + box[3] * canvasArea.ui) - y
                width: canvasArea.pixelAligned(canvasArea.mapX(box[0]) + box[2] * canvasArea.ui) - x
                x: canvasArea.pixelAligned(canvasArea.mapX(box[0]))
                y: canvasArea.pixelAligned(canvasArea.mapY(box[1], box[0]))

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
