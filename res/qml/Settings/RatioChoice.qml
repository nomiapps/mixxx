import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "../Theme"
import ".." as Skin

// A segmented choice. Drawn as the chrome draws a sunken panel with a selected
// row: hairline track, the chosen segment in the Theme.blue tint with a blue
// hairline. It used to be a pill with an inner-shadow glow on a solid cyan
// segment under a drop shadow, a shape nothing else in the New UI has.
Item {
    id: root

    readonly property real cellSize: {
        Math.max.apply(null, options.map(option => fontMetrics.advanceWidth(option))) + root.spacing * 2;
    }
    property alias content: contentList
    property color inactiveColor: Theme.sunkenBackgroundColor
    property real maxWidth: 0
    property alias metric: fontMetrics
    property bool normalizedWidth: true
    required property list<string> options
    property var selected: options.length ? options[0] : null
    property real spacing: 9
    property list<var> tooltips: []

    implicitHeight: contentList.visible ? contentList.height : contentSpin.height
    implicitWidth: contentList.visible ? contentList.width : contentSpin.width

    onTooltipsChanged: {
        popup.close();
    }

    FontMetrics {
        id: fontMetrics

        font.bold: true
        font.capitalization: Font.AllUppercase
        font.pixelSize: 11
    }
    Rectangle {
        id: contentList

        anchors.centerIn: parent
        border.color: Theme.panelBorderColor
        border.width: 1
        color: root.inactiveColor
        height: 26
        radius: 4
        visible: root.maxWidth == 0 || root.maxWidth > root.cellSize * root.options.length
        width: {
            if (root.normalizedWidth) {
                root.cellSize * root.options.length + 4;
            } else {
                options.reduce((acc, option) => acc + fontMetrics.advanceWidth(option) + root.spacing * 2, 0) + 4;
            }
        }

        Row {
            anchors.fill: parent
            anchors.margins: 2

            Repeater {
                model: options

                Rectangle {
                    id: contentOption

                    readonly property bool current: root.selected == modelData
                    required property int index
                    required property var modelData

                    border.color: Theme.blue
                    border.width: current ? 1 : 0
                    color: current ? Theme.selectionColor : (optionHover.hovered ? Theme.hoverWashColor : "transparent")
                    height: parent.height
                    radius: 3
                    width: root.normalizedWidth ? root.cellSize : fontMetrics.advanceWidth(modelData) + root.spacing * 2

                    Text {
                        anchors.fill: parent
                        color: contentOption.current ? Theme.white : Theme.deckTextColor
                        font: fontMetrics.font
                        horizontalAlignment: Text.AlignHCenter
                        text: contentOption.modelData
                        verticalAlignment: Text.AlignVCenter
                    }
                    HoverHandler {
                        id: optionHover

                        onHoveredChanged: {
                            if (!root.tooltips[contentOption.index]) {
                                return;
                            }
                            if (hovered) {
                                popup.tooltip = root.tooltips[contentOption.index] || "";
                                popup.x = Qt.binding(function () {
                                    return contentOption.mapToItem(root, 0, 0).x + contentOption.width / 2 - popup.width / 2;
                                });
                                popup.open();
                            } else {
                                popup.close();
                            }
                        }
                    }
                    TapHandler {
                        onTapped: {
                            root.selected = contentOption.modelData;
                        }
                    }
                }
            }
        }
    }
    SpinBox {
        id: contentSpin

        property real textWidth: fontMetrics.advanceWidth(root.options.reduce((accumulator, currentValue) => accumulator.length > currentValue.length ? accumulator : currentValue, ""))

        anchors.centerIn: parent
        font: fontMetrics.font
        from: 0
        padding: 0
        spacing: root.spacing
        textFromValue: function (value) {
            return root.options[value];
        }
        to: root.options.length - 1
        value: root.options.indexOf(root.selected)
        valueFromText: function (text) {
            for (var i = 0; i < root.options.length; ++i) {
                if (root.options[i].toLowerCase().indexOf(text.toLowerCase()) === 0)
                    return i;
            }
            return contentSpin.value;
        }
        visible: !contentList.visible

        background: Rectangle {
            border.color: Theme.panelBorderColor
            border.width: 1
            color: root.inactiveColor
            implicitHeight: 26
            implicitWidth: contentSpin.textWidth + 2 * contentSpin.spacing + 48
            radius: 4
        }
        contentItem: Text {
            color: Theme.white
            font: contentSpin.font
            horizontalAlignment: Text.AlignHCenter
            text: contentSpin.textFromValue(contentSpin.value, contentSpin.locale) ?? ""
            verticalAlignment: Text.AlignVCenter
        }
        down.indicator: Indicator {
            pressed: contentSpin.down.pressed
            text: "<"
            x: contentSpin.mirrored ? parent.width - width : 0
        }
        up.indicator: Indicator {
            pressed: contentSpin.up.pressed
            text: ">"
            x: contentSpin.mirrored ? 0 : parent.width - width
        }

        onValueChanged: {
            if (!contentSpin.visible)
                return;
            root.selected = contentSpin.textFromValue(value) ?? "";
            popup.tooltip = root.tooltips[contentSpin.value] ?? "";
            popup.x = contentSpin.width / 2 - popup.width / 2;
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            onEntered: {
                if (!root.tooltips[contentSpin.value])
                    return;
                popup.x = contentSpin.width / 2 - popup.width / 2;
                popup.open();
            }
            onExited: {
                popup.close();
            }
            onPressed: {
                mouse.accepted = false;
            }
        }
    }
    Popup {
        id: popup

        property string tooltip: ""

        closePolicy: Popup.NoAutoClose
        height: tooltip.implicitHeight + 15
        padding: 0
        width: Math.max(tooltip.implicitWidth * 1.5, 50)
        x: 0
        y: root.height

        background: Item {
        }
        contentItem: Item {
            Item {
                id: contentPopup

                anchors.fill: parent

                Shape {
                    // Qt 6.6+ resolution-independent antialiasing; the older
                    // geometry renderer stair-steps curves on some displays.
                    preferredRendererType: Shape.CurveRenderer
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    antialiasing: true
                    height: width
                    width: 20

                    ShapePath {
                        capStyle: ShapePath.RoundCap
                        fillColor: Theme.embeddedBackgroundColor
                        fillRule: ShapePath.WindingFill
                        startX: 10
                        startY: 0
                        strokeColor: Theme.deckBackgroundColor
                        strokeWidth: 2

                        PathLine {
                            x: 20
                            y: 10
                        }
                        PathLine {
                            x: 0
                            y: 10
                        }
                        PathLine {
                            x: 10
                            y: 0
                        }
                    }
                }
                Skin.EmbeddedBackground {
                    anchors.fill: parent
                    anchors.topMargin: 10

                    Text {
                        id: tooltip

                        anchors.centerIn: parent
                        color: Theme.white
                        text: popup.tooltip
                    }
                }
            }
            DropShadow {
                anchors.fill: parent
                color: "#000000"
                horizontalOffset: 0
                radius: 8.0
                source: contentPopup
                verticalOffset: 0
            }
        }
    }

    // The stepper ends of the narrow fallback: small flat faces inside the field.
    component Indicator: Item {
        id: indicator

        property bool pressed: false
        required property string text

        height: parent ? parent.height : 26
        implicitWidth: 24

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            color: indicator.pressed ? Theme.pressedWashColor : (indicatorHover.hovered ? Theme.hoverWashColor : Theme.controlFaceColor)
            radius: 3

            Text {
                anchors.fill: parent
                color: Theme.deckTextColor
                font.bold: true
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                text: indicator.text
                verticalAlignment: Text.AlignVCenter
            }
            HoverHandler {
                id: indicatorHover
            }
        }
    }
}
