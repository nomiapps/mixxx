pragma ComponentBehavior: Bound

import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Shapes
import "Theme"

// A live Camelot wheel. The 24 keys sit on twelve spokes, minor on the inner
// ring (the "A" letter) and major on the outer ("B"), 1 at twelve o'clock and
// the numbers running clockwise -- the arrangement every Camelot/Lancelot
// chart uses, which is the Circle of Fifths rotated so that neighbouring
// wedges are a fifth apart.
//
// What makes it worth more than the static chart it replaces: it reads the
// decks. Every deck holding a track with a detected key marks its wedge, and
// the wedges that key mixes into are lit, so "where can I go next" is answered
// by looking instead of by counting round the circle.
Item {
    id: root

    // Parsed form of activeQuery: {text, compatible} for "key:X" / "~key:X",
    // null for anything else (or nothing).
    readonly property var activeFilter: {
        const m = root.activeQuery.trim().match(/^(~?)key:(\S+)$/i);
        return m ? ({
                "text": m[2].toLowerCase(),
                "compatible": m[1] === "~"
            }) : null;
    }

    // The library's current search text. When it is a key search this marks
    // the wedge it targets, and a second click on that wedge clears it. The
    // library owns the text; this is a read-only mirror.
    property string activeQuery: ""
    // Set once the deck list is clicked, after which the highlight stays where
    // it was put instead of following the next track load.
    property bool aimedByUser: false

    // The sounding key of each deck, one entry per deck, 0 where a deck is
    // empty or unanalysed. Deck trackers push into this rather than the wheel
    // polling them: a binding that walks Repeater.itemAt() only re-runs by
    // luck, whereas reassigning the whole array re-runs every reader.
    property var deckKeys: new Array(root.numDecks).fill(0)
    // Wedge index under the pointer (same indexing as the wedge Repeater:
    // 0-11 major, 12-23 minor), -1 for none.
    property int hoveredIndex: -1

    // Camelot number -> [major, minor] as ChromaticKey values (keys.proto),
    // index 0 unused because Camelot numbers start at 1. This mirrors
    // s_openKeyToKeys in keyutils.cpp re-indexed from OpenKey to Camelot
    // (Camelot n is OpenKey n+5, which is why 8B comes out as C major):
    //   1 B/G#m   2 F#/Ebm   3 Db/Bbm   4 Ab/Fm    5 Eb/Cm   6 Bb/Gm
    //   7 F/Dm    8 C/Am     9 G/Em    10 D/Bm    11 A/F#m  12 E/C#m
    readonly property var keysOf: [[], [12, 21], [7, 16], [2, 23], [9, 18], [4, 13], [11, 20], [6, 15], [1, 22], [8, 17], [3, 24], [10, 19], [5, 14]]
    property int numDecks: 4
    // Ring geometry, as fractions of `radius`. The majors are the outer band,
    // the minors the inner one, and rHub is where the deck list starts.
    readonly property real rHub: 0.49
    readonly property real rMajorInner: 0.76
    readonly property real rMajorOuter: 0.99
    readonly property real rMinorInner: 0.51
    readonly property real rMinorOuter: 0.74
    readonly property real radius: Math.min(width, height) / 2
    // Deck the compatibility highlight is aimed at. 0 when no deck holds a
    // key, which leaves the wheel as a plain reference chart with nothing
    // dimmed.
    property int selectedDeck: 0
    readonly property bool selectedIsMajor: root.isMajor(root.selectedKey)
    readonly property int selectedKey: root.selectedDeck > 0 ? (root.deckKeys[root.selectedDeck - 1] ?? 0) : 0
    readonly property int selectedNumber: root.numberOf(root.selectedKey)
    // A hairline between wedges, so the ring reads as twelve segments rather
    // than one disc.
    readonly property real wedgeGap: 1.6
    readonly property real wedgeSweep: 30

    // A wedge was clicked: `code` is its Camelot code, `compatible` says the
    // click was on the aimed deck's own wedge, which asks for everything that
    // mixes with it rather than the one key.
    signal keyClicked(string code, bool compatible)

    // Centre of a wedge in the degrees Shapes wants: 0 at three o'clock,
    // growing clockwise, which puts Camelot 1 at the top.
    function angleOf(number) {
        return -90 + (number - 1) * root.wedgeSweep;
    }
    // Aim at the lowest-numbered deck holding a key, so the wheel shows
    // something the moment it opens without waiting for a click. Lowest rather
    // than first-to-arrive: track loads finish asynchronously, so "first" put
    // the highlight on whichever deck happened to win the race.
    function autoAim() {
        for (let i = 0; i < root.deckKeys.length; ++i) {
            if (root.deckKeys[i] > 0) {
                root.selectedDeck = i + 1;
                return;
            }
        }
        root.selectedDeck = 0;
    }
    // How well a wedge mixes with the selection:
    //   2 = the classic Camelot moves -- stay put, swap letter on the same
    //       number, or step one number keeping the letter.
    //   1 = the diagonal steps Mixxx also treats as compatible
    //       (KeyUtils::getCompatibleKeys, after Phil Morse): a number step and
    //       a letter swap at once.
    //   0 = not compatible.
    function compatibility(number, major) {
        if (root.selectedNumber <= 0)
            return 0;

        if (number === root.selectedNumber)
            return 2;

        if (number !== root.wrap(root.selectedNumber + 1) && number !== root.wrap(root.selectedNumber - 1))
            return 0;

        return major === root.selectedIsMajor ? 2 : 1;
    }
    // How many decks are parked on a key -- drives the wedge outline.
    function deckCountOn(key) {
        if (key <= 0)
            return 0;

        let count = 0;
        for (let i = 0; i < root.deckKeys.length; ++i) {
            if (root.deckKeys[i] === key)
                ++count;
        }
        return count;
    }

    // ChromaticKey 1..12 are the majors and 13..24 the minors, so the mode is
    // just which half of the enum the value falls in.
    function isMajor(key) {
        return key >= 1 && key <= 12;
    }
    // Every spelling of a key the library might be filtering on: the wheel's
    // own Camelot code, the app's current notation (what the track context
    // menu sends), plus Open Key and traditional so a typed search matches too.
    function keyMatchesFilter(key) {
        if (!root.activeFilter)
            return false;

        const openKey = 2;
        const lancelot = 3;
        const traditional = 4;
        const spellings = [openKey, lancelot, traditional, keyNotationControl.value].map(n => Mixxx.KeyUtils.keyToString(key, n).toLowerCase());
        return spellings.indexOf(root.activeFilter.text) !== -1;
    }
    function numberOf(key) {
        if (key <= 0)
            return 0;

        for (let n = 1; n <= 12; ++n) {
            if (root.keysOf[n][0] === key || root.keysOf[n][1] === key)
                return n;
        }
        return 0;
    }
    function setDeckKey(index, key) {
        if (root.deckKeys[index] === key)
            return;

        const next = root.deckKeys.slice();
        next[index] = key;
        root.deckKeys = next;
        if (!root.aimedByUser)
            root.autoAim();
        else if (key <= 0 && root.selectedDeck === index + 1)
            root.selectedDeck = 0;
    }
    // Inverse of the wedge geometry: which wedge index (Repeater order) sits
    // under a point of the wheel item, or -1 for the hub, the gaps between
    // rings and anything outside. Done as one polar lookup on a single
    // MouseArea, because 24 rectangular MouseAreas over 24 arcs would each
    // claim a whole quadrant.
    function wedgeAt(x, y) {
        const dx = x - root.radius;
        const dy = y - root.radius;
        const r = Math.hypot(dx, dy) / root.radius;
        let major;
        if (r >= root.rMajorInner && r <= root.rMajorOuter)
            major = true;
        else if (r >= root.rMinorInner && r <= root.rMinorOuter)
            major = false;
        else
            return -1;
        const degrees = Math.atan2(dy, dx) * 180 / Math.PI;
        const number = root.wrap(Math.round((degrees + 90) / root.wedgeSweep) + 1);
        return (major ? 0 : 12) + number - 1;
    }
    // Hue ring: one twelfth of the spectrum per spoke, so adjacent (mixable)
    // wedges are adjacent colours and the tritone sits opposite. Major reads
    // lighter than the relative minor sharing its spoke.
    function wedgeColor(number, major) {
        if (number <= 0)
            return Theme.darkGray3;

        return Qt.hsla((number - 1) / 12, major ? 0.62 : 0.66, major ? 0.60 : 0.42, 1);
    }
    // Wrap a Camelot number into 1..12 so +1/-1 steps stay on the wheel.
    function wrap(number) {
        return ((number - 1 + 12) % 12) + 1;
    }

    // One tracker per deck. [ChannelN] key is the *sounding* key, so the wheel
    // follows the pitch fader rather than showing what the file was analysed
    // as; track_loaded gates it because the control keeps its last value while
    // a deck sits empty.
    Repeater {
        model: root.numDecks

        Item {
            id: deckItem

            readonly property string group: "[Channel" + (deckItem.index + 1) + "]"
            required property int index
            readonly property int key: loadedControl.value > 0 ? Math.round(keyControl.value) : 0

            Component.onCompleted: root.setDeckKey(deckItem.index, deckItem.key)
            onKeyChanged: root.setDeckKey(deckItem.index, deckItem.key)

            Mixxx.ControlProxy {
                id: keyControl

                group: deckItem.group
                key: "key"
            }
            Mixxx.ControlProxy {
                id: loadedControl

                group: deckItem.group
                key: "track_loaded"
            }
        }
    }
    // Notation for the second line of each wedge, so the wheel speaks whatever
    // the rest of the app was told to speak.
    Mixxx.ControlProxy {
        id: keyNotationControl

        group: "[Library]"
        key: "key_notation"
    }
    Item {
        id: wheel

        anchors.centerIn: parent
        height: root.radius * 2
        width: root.radius * 2

        // Sits under the wedges, which take no mouse input themselves, so
        // every press on a wedge falls through to here. The hub's own
        // MouseAreas are above it and wedgeAt() returns -1 inside the hub
        // anyway, so the deck list keeps its clicks.
        MouseArea {
            anchors.fill: parent
            cursorShape: root.hoveredIndex >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
            hoverEnabled: true

            onClicked: mouse => {
                const index = root.wedgeAt(mouse.x, mouse.y);
                if (index < 0)
                    return;

                const number = index % 12 + 1;
                const major = index < 12;
                const key = root.keysOf[number][major ? 0 : 1];
                root.keyClicked(number + (major ? "B" : "A"), root.selectedKey > 0 && key === root.selectedKey);
            }
            onExited: root.hoveredIndex = -1
            onPositionChanged: mouse => root.hoveredIndex = root.wedgeAt(mouse.x, mouse.y)
        }
        // One delegate per key: its wedge and its label. ShapePath is not an
        // Item, so it cannot be a Repeater delegate on its own -- each wedge
        // gets its own Shape instead.
        Repeater {
            model: 24

            Item {
                id: wedge

                readonly property color base: root.wedgeColor(wedge.number, wedge.major)
                readonly property int compatibility: root.compatibility(wedge.number, wedge.major)
                readonly property int deckCount: root.deckCountOn(wedge.key)
                readonly property bool filtering: root.keyMatchesFilter(wedge.key)
                readonly property bool hovered: root.hoveredIndex === wedge.index
                required property int index
                readonly property real inner: root.radius * (wedge.major ? root.rMajorInner : root.rMinorInner)
                readonly property bool isSelected: root.selectedKey > 0 && wedge.key === root.selectedKey
                readonly property int key: root.keysOf[wedge.number][wedge.major ? 0 : 1]
                // Nothing selected means nothing is dimmed.
                readonly property bool lit: root.selectedNumber <= 0 || wedge.compatibility > 0
                readonly property bool major: wedge.index < 12
                readonly property int number: wedge.index % 12 + 1
                readonly property real outer: root.radius * (wedge.major ? root.rMajorOuter : root.rMinorOuter)
                readonly property real start: root.angleOf(wedge.number) - root.wedgeSweep / 2 + root.wedgeGap / 2
                readonly property real sweep: root.wedgeSweep - root.wedgeGap

                anchors.fill: parent

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        fillColor: {
                            let color;
                            switch (root.selectedNumber <= 0 ? 2 : wedge.compatibility) {
                            case 2:
                                color = wedge.base;
                                break;
                            case 1:
                                color = Qt.darker(wedge.base, 1.6);
                                break;
                            default:
                                color = Qt.alpha(Qt.darker(wedge.base, 2.8), 0.5);
                            }
                            // Hover lifts even a dimmed wedge, since a key
                            // that does not mix is still one you can look up.
                            return wedge.hovered ? Qt.lighter(color, 1.2) : color;
                        }
                        // Outline precedence: the wedge the library is filtered
                        // on (blue, the chrome's selection colour), then the
                        // aimed deck's wedge, then any wedge a deck is on.
                        strokeColor: wedge.filtering ? Theme.blue : (wedge.isSelected ? Theme.pureWhite : (wedge.deckCount > 0 ? Theme.offWhite : "transparent"))
                        strokeWidth: wedge.filtering || wedge.isSelected ? 2.5 : (wedge.deckCount > 0 ? 1.5 : 0)

                        PathAngleArc {
                            centerX: root.radius
                            centerY: root.radius
                            moveToStart: true
                            radiusX: wedge.outer
                            radiusY: wedge.outer
                            startAngle: wedge.start
                            sweepAngle: wedge.sweep
                        }
                        PathAngleArc {
                            centerX: root.radius
                            centerY: root.radius
                            moveToStart: false
                            radiusX: wedge.inner
                            radiusY: wedge.inner
                            startAngle: wedge.start + wedge.sweep
                            sweepAngle: -wedge.sweep
                        }
                    }
                }
                // Labels ride on top: Shapes carries no text, and upright text
                // stays readable where radial text would need the head tilted
                // for half the wheel.
                Item {
                    readonly property real mid: root.radius * (wedge.major ? (root.rMajorOuter + root.rMajorInner) / 2 : (root.rMinorOuter + root.rMinorInner) / 2)
                    readonly property real radians: root.angleOf(wedge.number) * Math.PI / 180

                    height: 1
                    width: 1
                    x: root.radius + mid * Math.cos(radians)
                    y: root.radius + mid * Math.sin(radians)

                    Column {
                        anchors.centerIn: parent
                        opacity: wedge.lit ? 1 : 0.4
                        spacing: -1

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 120
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Theme.darkGray
                            font.bold: true
                            font.family: Theme.fontFamily
                            font.pixelSize: Math.max(9, root.radius * 0.075)
                            text: wedge.number + (wedge.major ? "B" : "A")
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: Qt.alpha(Theme.darkGray, 0.75)
                            font.family: Theme.fontFamily
                            font.pixelSize: Math.max(7, root.radius * 0.052)
                            // The chosen notation, unless that is one of the
                            // Lancelot variants -- those would just reprint
                            // the Camelot code above and waste the line.
                            text: {
                                const lancelot = 3;
                                const traditional = 4;
                                const lancelotAndTraditional = 6;
                                const notation = keyNotationControl.value;
                                const useNotation = (notation === lancelot || notation === lancelotAndTraditional) ? traditional : notation;
                                return Mixxx.KeyUtils.keyToString(wedge.key, useNotation);
                            }
                        }
                    }
                }
            }
        }
        // Deck markers sit outside their ring so they never land on a label.
        Repeater {
            model: root.numDecks

            Item {
                id: marker

                readonly property bool aimed: root.selectedDeck === marker.index + 1
                required property int index
                readonly property int key: root.deckKeys[marker.index] ?? 0
                readonly property bool major: root.isMajor(marker.key)
                readonly property int number: root.numberOf(marker.key)
                readonly property real radians: (root.angleOf(marker.number) + (marker.slot - (root.deckCountOn(marker.key) - 1) / 2) * 8) * Math.PI / 180
                readonly property real ring: root.radius * (marker.major ? root.rMajorOuter + 0.06 : (root.rMajorInner + root.rMinorOuter) / 2)
                // Decks sharing a wedge would stack, so fan them along the arc
                // by their position among the decks already on that key.
                readonly property int slot: {
                    let slot = 0;
                    for (let i = 0; i < marker.index; ++i) {
                        if (root.deckKeys[i] === marker.key)
                            ++slot;
                    }
                    return slot;
                }

                height: 1
                visible: marker.number > 0
                width: 1
                x: root.radius + ring * Math.cos(radians)
                y: root.radius + ring * Math.sin(radians)

                Rectangle {
                    anchors.centerIn: parent
                    border.color: marker.aimed ? Theme.pureWhite : Theme.darkGray
                    border.width: marker.aimed ? 2 : 1
                    color: Theme.backgroundColor
                    height: width
                    radius: width / 2
                    width: Math.max(14, root.radius * 0.1)

                    Text {
                        anchors.centerIn: parent
                        color: marker.aimed ? Theme.pureWhite : Theme.lightGray2
                        font.bold: true
                        font.family: Theme.fontFamily
                        font.pixelSize: parent.width * 0.6
                        text: marker.index + 1
                    }
                }
            }
        }
        // The hub doubles as the deck list: what each deck is in, and a click
        // to aim the highlight at it.
        Rectangle {
            anchors.centerIn: parent
            border.color: Theme.panelBorderColor
            border.width: 1
            color: Theme.backgroundColor
            height: width
            radius: width / 2
            width: root.radius * root.rHub * 2

            Column {
                anchors.centerIn: parent
                spacing: 2
                width: parent.width * 0.76

                Repeater {
                    model: root.numDecks

                    Rectangle {
                        id: chip

                        readonly property bool aimed: root.selectedDeck === chip.index + 1 && chip.key > 0
                        required property int index
                        readonly property int key: root.deckKeys[chip.index] ?? 0
                        readonly property bool major: root.isMajor(chip.key)
                        readonly property int number: root.numberOf(chip.key)

                        color: chip.aimed ? Theme.selectionColor : (chipArea.containsMouse ? Theme.hoverWashColor : "transparent")
                        height: Math.max(18, root.radius * 0.11)
                        radius: 3
                        width: parent.width

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: chip.aimed ? Theme.pureWhite : Theme.midGray3
                                font.bold: true
                                font.family: Theme.fontFamily
                                font.pixelSize: chip.height * 0.5
                                text: chip.index + 1
                            }
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.wedgeColor(chip.number, chip.major)
                                height: width
                                radius: 2
                                width: chip.height * 0.34
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: chip.key > 0 ? Theme.offWhite : Theme.midGray3
                                font.bold: true
                                font.family: Theme.fontFamily
                                font.pixelSize: chip.height * 0.5
                                text: chip.number > 0 ? chip.number + (chip.major ? "B" : "A") : "--"
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.lightGray3
                                font.family: Theme.fontFamily
                                font.pixelSize: chip.height * 0.42
                                text: chip.key > 0 ? Mixxx.KeyUtils.keyToString(chip.key, 4 /* Traditional */) : ""
                            }
                        }
                        MouseArea {
                            id: chipArea

                            anchors.fill: parent
                            enabled: chip.key > 0
                            hoverEnabled: true

                            // Clicking the aimed deck again releases the
                            // manual aim back to the automatic pick.
                            onClicked: {
                                if (chip.aimed) {
                                    root.aimedByUser = false;
                                    root.autoAim();
                                } else {
                                    root.aimedByUser = true;
                                    root.selectedDeck = chip.index + 1;
                                }
                            }
                        }
                    }
                }
            }
            // Says what a click does, and what the library is currently
            // filtered on when that came from here or the track menu.
            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: parent.height * 0.13
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.activeFilter ? Theme.lightGray3 : Theme.midGray3
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Math.max(8, root.radius * 0.036)
                horizontalAlignment: Text.AlignHCenter
                text: root.activeFilter ? qsTr("library: %1").arg(root.activeQuery.trim()) : qsTr("click a key to filter the library")
                width: parent.width * 0.7
            }
        }
    }
}
