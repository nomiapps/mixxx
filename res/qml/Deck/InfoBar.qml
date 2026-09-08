import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Shapes
import QtQuick.Layouts
import QtQml.Models // DelegateChoice for Qt >= 6.9
import Qt.labs.qmlmodels // DelegateChooser
import "../Theme"

Rectangle {
    id: root

    property list<string> availableData: ["none", "title", "year", "time", "key", "duration", "artist", "rating"]
    readonly property var currentTrack: deckPlayer?.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property bool editMode: false
    required property string group
    property color lineColor: Theme.deckLineColor
    property bool minimized: false
    required property int rightColumnWidth
    // Every cell after the first is this wide, in both rows, so the separators
    // sit at the same x in each and the bar reads as columns rather than two
    // unrelated strips. The time cell used to size itself instead, which is
    // what pulled the rows out of line.
    readonly property int columnWidth: Math.max(rightColumnWidth, timeColumnWidth)
    // The two columns between the title and the time only ever hold a key, a
    // year or a duration -- a handful of characters. At the time column's
    // width they took a third of the bar for it, so they are fixed narrow and
    // the title keeps the rest.
    readonly property int narrowColumnWidth: 72
    // The time is the only cell whose content dictates a width: it is the
    // longest string in the bar and must not elide. The time delegate reports
    // what it needs here and every column follows it.
    property int timeColumnWidth: 136

    // Column widths by index, shared by both rows and edit mode so the
    // separators keep lining up: the first fills, the last follows the time,
    // the ones between are narrow. Both rows have the same number of columns.
    function widthForColumn(index) {
        if (index === 0)
            return 0;
        return index === topRowModel.count - 1 ? root.columnWidth : root.narrowColumnWidth;
    }

    border.color: "#30343d"
    border.width: 1
    radius: 6

    gradient: Gradient {
        orientation: Gradient.Horizontal

        GradientStop {
            color: {
                const trackColor = root.currentTrack?.color;
                if (!trackColor.valid)
                    return Theme.deckInfoBarBackgroundColor;

                return Qt.darker(root.currentTrack?.color, 3.2);
            }
            position: 0
        }
        GradientStop {
            color: Theme.deckInfoBarBackgroundColor
            position: 1
        }
    }

    Image {
        id: coverArt

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        asynchronous: true
        // Cover art arrives at whatever size the file embeds -- often 1000 px
        // or more -- and is minified to the bar's height. Without mipmaps that
        // is a straight bilinear sample and the art shimmers and aliases.
        mipmap: true
        source: root.currentTrack?.coverArtUrl
        visible: false
        width: height
    }
    Rectangle {
        id: coverArtCircle

        anchors.fill: coverArt
        color: Theme.deckEmptyCoverArt
        radius: root.radius - 1
        visible: !root.deckPlayer?.isLoaded && !root.minimized
    }
    OpacityMask {
        id: coverArtMask

        anchors.fill: coverArt
        maskSource: coverArtCircle
        source: coverArt
        visible: root.deckPlayer?.isLoaded && !root.minimized

        Skin.FadeBehavior on visible {
            fadeTarget: coverArtMask
        }
    }
    DelegateChooser {
        id: cellDelegate

        role: "type"

        DelegateChoice {
            roleValue: "title"

            Cell {
                id: titleCell

                // A title wider than its cell scrolls instead of eliding. The
                // stock label still draws the unloaded state and any title
                // that fits; the marquee only takes over when there is
                // something to show that the label would cut off.
                readonly property bool overflowing: !root.editMode && !!root.deckPlayer?.isLoaded && scrollText.implicitWidth > scroller.width + 1
                readonly property string titleText: root.deckPlayer?.isLoaded ? (root.currentTrack?.title ?? "") : "No track loaded"

                item.font.bold: false
                item.font.weight: root.deckPlayer?.isLoaded ? Font.DemiBold : Font.Thin
                item.text: titleCell.titleText
                item.visible: !titleCell.overflowing

                onOverflowingChanged: strip.x = 0
                // The loop restarts from the front whenever the title changes,
                // so a new track is readable before it starts to move.
                onTitleTextChanged: {
                    strip.x = 0;
                    if (scrollAnimation.running)
                        scrollAnimation.restart();
                }

                Item {
                    id: scroller

                    anchors.fill: parent
                    anchors.rightMargin: 6
                    clip: true
                    visible: titleCell.overflowing

                    // Two copies one gap apart make the loop seamless: when the
                    // first has scrolled out, the second sits exactly where the
                    // first began, and the strip snaps back unnoticed.
                    Row {
                        id: strip

                        readonly property int gap: 48

                        anchors.verticalCenter: parent.verticalCenter
                        spacing: gap

                        Skin.EmbeddedText {
                            id: scrollText

                            color: Theme.white
                            elide: Text.ElideNone
                            font: titleCell.item.font
                            text: titleCell.titleText
                        }
                        Skin.EmbeddedText {
                            color: Theme.white
                            elide: Text.ElideNone
                            font: titleCell.item.font
                            text: titleCell.titleText
                        }
                    }
                    SequentialAnimation {
                        id: scrollAnimation

                        // Pixels per second; slow enough to read as it goes by.
                        readonly property real speed: 40

                        loops: Animation.Infinite
                        running: titleCell.overflowing

                        PauseAnimation {
                            duration: 2500
                        }
                        NumberAnimation {
                            duration: (scrollText.width + strip.gap) / scrollAnimation.speed * 1000
                            from: 0
                            property: "x"
                            target: strip
                            to: -(scrollText.width + strip.gap)
                        }
                    }
                }
            }
        }
        DelegateChoice {
            roleValue: "artist"

            Cell {
                item.text: root.currentTrack?.artist
            }
        }
        DelegateChoice {
            roleValue: "year"

            Cell {
                // A track with no year keeps its column and shows nothing.
                // Dropping the cell instead moved every separator after it, so
                // the two rows only lined up on tracks that happened to have a
                // year. Visibility decides the layout; emptiness is the text's
                // business.
                item.text: root.currentTrack?.year ?? ""
                visible: root.width > 500
            }
        }
        DelegateChoice {
            roleValue: "time"

            Item {
                id: timeCell

                required property int index

                Layout.fillHeight: true
                Layout.minimumWidth: 96
                Layout.preferredWidth: root.widthForColumn(timeCell.index)
                // Paired with the rating below it. Separators only draw on a
                // loaded deck, so the rows never show a mismatch while empty.
                visible: root.width > 400 && !!root.deckPlayer?.isLoaded

                // Tell the bar how wide the time needs to be; every column then
                // takes at least that, so the two rows line up.
                Binding {
                    property: "timeColumnWidth"
                    target: root
                    value: Math.max(136, timeLabel.implicitWidth + 16)
                }

                TrackTime {
                    id: timeLabel

                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    color: Theme.white
                    display: Mixxx.Config.controlPositionDisplay
                    elide: Text.ElideNone
                    font.bold: true
                    font.pixelSize: 13
                    group: root.group
                    horizontalAlignment: Text.AlignHCenter
                    mode: Mixxx.Config.controlTimeFormat
                    wrapMode: Text.NoWrap
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: Mixxx.Config.controlPositionDisplay = (Mixxx.Config.controlPositionDisplay + 1) % 3
                }
                Rectangle {
                    anchors.bottom: timeCell.bottom
                    anchors.right: timeCell.right
                    anchors.top: timeCell.top
                    anchors.topMargin: 5
                    color: root.lineColor
                    opacity: 0.7
                    // `parent` here is timeCell, which carries index but no model,
                    // so parent.model.count threw a TypeError on every evaluation --
                    // continuously, on every deck. The sibling bindings at the
                    // delegate roots below are fine because THEIR parent is the
                    // model-carrying layout. Reach it explicitly, guard the lookup,
                    // and restore the isLoaded condition those siblings still have.
                    visible: timeCell.index !== (timeCell.parent?.model?.count ?? 0) - 1 && !!root.deckPlayer?.isLoaded
                    width: 1
                }
            }
        }
        DelegateChoice {
            // An empty column. "none" was already offered in edit mode but had
            // no delegate, so picking it dropped the cell entirely and shifted
            // everything after it; it also gives a row with fewer fields than
            // the other a spacer, so the two stay in step.
            roleValue: "none"

            Cell {
                item.text: ""
            }
        }
        DelegateChoice {
            roleValue: "key"

            Cell {
                // Read from the deck's control rather than the track's stored
                // text, so it follows a live key change and re-prints itself
                // when the notation setting changes instead of on next load.
                item.text: {
                    if (!root.deckPlayer?.isLoaded || keyControl.value <= 0) {
                        return "";
                    }
                    return Mixxx.KeyUtils.keyToString(keyControl.value, keyNotationControl.value);
                }
                visible: root.deckPlayer?.isLoaded

                Mixxx.ControlProxy {
                    id: keyControl

                    group: root.group
                    key: "key"
                }
                Mixxx.ControlProxy {
                    id: keyNotationControl

                    group: "[Library]"
                    key: "key_notation"
                }
            }
        }
        DelegateChoice {
            roleValue: "duration"

            Cell {
                item.elide: Text.ElideNone
                item.font.pixelSize: 13
                item.text: {
                    const seconds = durationSeconds.value;
                    if (!Number.isFinite(seconds) || seconds <= 0) {
                        return "";
                    }
                    return Mixxx.DurationFormatter.format(seconds, TrackTime.Mode.TraditionalCoarse);
                }
                // Same threshold as the year above it: paired columns come and
                // go together, or the rows stop lining up as the deck narrows.
                visible: root.width > 500

                Mixxx.ControlProxy {
                    id: durationSeconds

                    group: root.group
                    key: "duration"
                }
            }
        }
        DelegateChoice {
            roleValue: "rating"

            Item {
                id: cell

                required property int index
                property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)
                property bool showSeparator: index != parent.model.count - 1 && root.deckPlayer?.isLoaded

                Layout.fillHeight: true
                Layout.fillWidth: index == 0
                Layout.preferredWidth: root.widthForColumn(index)
                visible: root.width > 400

                Mixxx.ControlProxy {
                    id: rateRatioControl

                    group: root.group
                    key: "rate_ratio"
                }
                Row {
                    id: stars

                    anchors.centerIn: parent
                    spacing: 0
                    visible: root.deckPlayer?.isLoaded

                    Repeater {
                        model: 5

                        Shape {
                            // Qt 6.6+ resolution-independent antialiasing; the older
                            // geometry renderer stair-steps curves on some displays.
                            preferredRendererType: Shape.CurveRenderer
                            id: star

                            antialiasing: true
                            height: 14
                            width: 16

                            ShapePath {
                                fillColor: mouse.containsMouse && !(mouse.pressedButtons & Qt.RightButton) && mouse.mouseX > star.x + stars.x ? "#3a60be" : (!mouse.containsMouse || mouse.pressedButtons & Qt.RightButton) && root.currentTrack?.stars > index ? (mouse.containsMouse ? "#7D3B3B" : '#D9D9D9') : '#96d9d9d9'
                                startX: 8
                                startY: 0
                                strokeColor: 'transparent'

                                PathLine {
                                    x: 9.78701
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 15.3496
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 10.8494
                                    y: 8.38525
                                }
                                PathLine {
                                    x: 12.5683
                                    y: 13.5676
                                }
                                PathLine {
                                    x: 8.06808
                                    y: 10.3647
                                }
                                PathLine {
                                    x: 3.56787
                                    y: 13.5676
                                }
                                PathLine {
                                    x: 5.2868
                                    y: 8.38525
                                }
                                PathLine {
                                    x: 0.786587
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 6.34915
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 8.06808
                                    y: 0
                                }
                            }
                        }
                    }
                }
                MouseArea {
                    id: mouse

                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: event => {
                        if (!root.currentTrack) {
                            return;
                        }
                        let selectedStars = Math.ceil((mouseX - stars.x) / 16);
                        if (event.button === Qt.RightButton) {
                            root.currentTrack.stars = 0;
                        } else if (selectedStars >= 0 && selectedStars <= 5) {
                            root.currentTrack.stars = selectedStars;
                        }
                    }
                }
                Rectangle {
                    id: separator

                    anchors.bottom: cell.bottom
                    anchors.right: cell.right
                    anchors.top: cell.top
                    anchors.topMargin: 5
                    color: root.lineColor
                    opacity: 0.7
                    visible: showSeparator
                    width: 1

                    Skin.FadeBehavior on visible {
                        fadeTarget: separator
                    }
                }
            }
        }
    }
    ListModel {
        id: topRowModel

        ListElement {
            type: "title"
        }
        // Spacer above the key, so both rows have four columns and their
        // separators fall on the same lines. Year still sits above duration
        // and the time above the rating, as before.
        ListElement {
            type: "none"
        }
        ListElement {
            type: "year"
        }
        ListElement {
            type: "time"
        }
    }
    ListModel {
        id: bottomRowModel

        ListElement {
            type: "artist"
        }
        ListElement {
            type: "key"
        }
        ListElement {
            type: "duration"
        }
        ListElement {
            type: "rating"
        }
    }
    Component {
        id: editCellDelegate

        Skin.ComboBox {
            required property int index
            readonly property var modelData: parent.model
            required property string type

            Layout.fillHeight: true
            Layout.fillWidth: index == 0
            Layout.preferredWidth: root.widthForColumn(index)
            currentIndex: root.availableData.indexOf(type)
            model: root.availableData

            onCurrentIndexChanged: {
                modelData.setProperty(index, "type", root.availableData[currentIndex]);
            }
        }
    }
    ColumnLayout {
        anchors.bottom: root.bottom
        anchors.left: coverArt.right
        anchors.right: root.right
        anchors.top: root.top
        spacing: 0

        RowLayout {
            id: topRow

            readonly property bool isTop: true
            property var model: topRowModel

            Layout.fillHeight: true
            Layout.fillWidth: true

            Repeater {
                delegate: root.editMode ? editCellDelegate : cellDelegate
                model: parent.model
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: Theme.accentColor
            height: 1
            opacity: root.deckPlayer?.isLoaded ? 0.8 : 0.35
            visible: !root.minimized
        }
        RowLayout {
            id: bottomRow

            property var model: bottomRowModel

            Layout.fillHeight: true
            Layout.fillWidth: true
            visible: !root.minimized

            Repeater {
                delegate: root.editMode ? editCellDelegate : cellDelegate
                model: parent.model
            }
        }
    }

    component Cell: Item {
        id: cell

        required property int index
        readonly property bool isTop: !!parent.isTop
        property alias item: data
        property bool showSeparator: index != parent.model.count - 1 && root.deckPlayer?.isLoaded

        Layout.fillHeight: true
        Layout.fillWidth: index == 0
        Layout.leftMargin: index == 0 ? 12 : 0
        Layout.preferredWidth: root.widthForColumn(index)

        Skin.EmbeddedText {
            id: data

            anchors.fill: parent
            color: Theme.white
            font.bold: isTop
            font.pixelSize: isTop ? 17 : Theme.textFontPixelSize
            horizontalAlignment: index == 0 ? Text.AlignLeft : Text.AlignHCenter
            visible: root.deckPlayer?.isLoaded

            Skin.FadeBehavior on visible {
                fadeTarget: data
            }
        }
        Rectangle {
            id: separator

            anchors.bottom: cell.bottom
            anchors.bottomMargin: isTop ? 0 : 5
            anchors.right: cell.right
            anchors.top: cell.top
            anchors.topMargin: isTop ? 5 : 0
            color: root.lineColor
            opacity: 0.7
            visible: showSeparator
            width: 1

            Skin.FadeBehavior on visible {
                fadeTarget: separator
            }
        }
    }
}
