import QtQuick
import QtQuick.Controls
import "../Theme"

// A number field with steppers. Drawn as the chrome draws a field: window
// ground behind a hairline that turns Theme.blue while it has focus, steppers
// as small flat faces inside it. It used to show the value on a solid cyan
// slab between shadowed squares, and although it said editable it had no text
// input to edit.
SpinBox {
    id: root

    readonly property int decimalFactor: Math.pow(10, decimals)
    property int decimals: root.precision ?? 0
    property double max: 1
    property double min: 0
    property int precision: 2
    // Shown instead of the value when set; the BPM precision preview uses it.
    property string previewText: ""
    property real realValue: 0
    property double step: 1 / decimalFactor
    property string suffix: ""
    // Width the widest value needs, so a caller can size the field to its range.
    readonly property real textWidth: widest.width

    function decimalToInt(decimal) {
        return decimal * decimalFactor;
    }

    editable: true
    from: decimalToInt(root.min)
    implicitHeight: 26
    padding: 0
    spacing: 2
    stepSize: root.step * decimalFactor
    textFromValue: function (value, locale) {
        return Number(value / decimalFactor).toLocaleString(locale, 'f', root.precision);
    }
    to: decimalToInt(root.max)
    value: decimalToInt(realValue)
    valueFromText: function (text, locale) {
        return Math.round(Number.fromLocaleString(locale, text) * decimalFactor);
    }

    TextMetrics {
        id: widest

        font: root.font
        text: root.textFromValue(root.to, root.locale)
    }
    // The suffix and the preview live in the background, not in the input's
    // text: SpinBox re-parses its TextInput whenever the text stops matching
    // displayText, so a suffix inside the text is a binding loop.
    background: Rectangle {
        border.color: root.activeFocus ? Theme.blue : Theme.panelBorderColor
        border.width: 1
        color: Theme.fieldBackgroundColor
        implicitWidth: Math.max(140, root.textWidth + suffixText.width + 2 * 24 + 24)
        radius: 4

        Text {
            id: suffixText

            anchors.right: parent.right
            anchors.rightMargin: 24 + 8
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextColor
            font: root.font
            text: root.suffix
            visible: root.suffix.length > 0
        }
        Text {
            anchors.centerIn: parent
            color: Theme.white
            font: root.font
            text: root.previewText
            visible: root.previewText.length > 0
        }
    }
    contentItem: TextInput {
        color: Theme.white
        font: root.font
        horizontalAlignment: Qt.AlignHCenter
        inputMethodHints: root.inputMethodHints
        readOnly: !root.editable
        rightPadding: suffixText.visible ? suffixText.width + 4 : 0
        selectByMouse: true
        selectedTextColor: Theme.white
        selectionColor: Theme.blue
        text: root.displayText
        validator: root.validator
        verticalAlignment: Qt.AlignVCenter
        visible: root.previewText.length === 0
    }
    down.indicator: Indicator {
        pressed: root.down.pressed
        text: "−"
        x: root.mirrored ? parent.width - width : 0
    }
    up.indicator: Indicator {
        pressed: root.up.pressed
        text: "+"
        x: root.mirrored ? 0 : parent.width - width
    }
    validator: DoubleValidator {
        bottom: Math.min(root.from, root.to)
        decimals: root.precision
        notation: DoubleValidator.StandardNotation
        top: Math.max(root.from, root.to)
    }

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
