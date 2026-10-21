// Two fingers on the New UI's own Slider (res/qml/Mixxx/Controls/Slider.qml, the base
// of Fader): both must move at once. Plus a second finger tapping a button, and a
// MouseArea, while the first holds a fader. Run offscreen with the deps bundle's
// qmltestrunner: QT_QPA_PLATFORM=offscreen qmltestrunner -input tools/qmltests/tst_multitouch.qml
import QtQuick
import QtQuick.Controls
import QtTest
import "../../res/qml/Mixxx/Controls" as MC

Item {
    id: root

    width: 600
    height: 300

    MC.Slider {
        id: left

        x: 50
        y: 20
        width: 40
        height: 200
        // As ControlFader does: what the user moved goes out through moved();
        // the value itself is bound elsewhere.
        property real movedTo: 0.2

        value: 0.2
        onMoved: v => movedTo = v
    }
    MC.Slider {
        id: right

        x: 200
        y: 20
        width: 40
        height: 200
        // As ControlFader does: what the user moved goes out through moved();
        // the value itself is bound elsewhere.
        property real movedTo: 0.2

        value: 0.2
        onMoved: v => movedTo = v
    }
    Button {
        id: button

        property int clicks: 0

        x: 350
        y: 20
        width: 80
        height: 60
        text: "cue"
        onClicked: clicks++
    }
    MouseArea {
        id: mouseArea

        property int clicks: 0

        x: 350
        y: 150
        width: 80
        height: 60
        onClicked: clicks++
    }

    TestCase {
        name: "MultiTouch"
        when: windowShown

        function init() {
            left.movedTo = 0.2;
            right.movedTo = 0.2;
            button.clicks = 0;
            mouseArea.clicks = 0;
        }

        function test_oneFaderByTouch() {
            const t = touchEvent(root);
            t.press(0, root, 70, 160).commit();
            for (let step = 1; step <= 10; ++step) {
                wait(16);
                t.move(0, root, 70, 160 - step * 8).commit();
            }
            wait(16);
            t.release(0, root, 70, 80).commit();
            wait(30);
            verify(left.movedTo > 0.5, "one fader: " + left.movedTo);
        }

        function test_twoFadersAtOnce() {
            const t = touchEvent(root);
            t.press(0, root, 70, 160).press(1, root, 220, 160).commit();
            for (let step = 1; step <= 10; ++step) {
                wait(16);
                t.move(0, root, 70, 160 - step * 8).move(1, root, 220, 160 - step * 8).commit();
            }
            wait(16);
            t.release(0, root, 70, 80).release(1, root, 220, 80).commit();
            wait(30);
            verify(left.movedTo > 0.5, "left fader: " + left.movedTo + " right: " + right.movedTo);
            verify(right.movedTo > 0.5, "right fader: " + right.movedTo);
        }

        function test_holdFaderTapButton() {
            const t = touchEvent(root);
            t.press(0, root, 70, 160).commit();
            for (let step = 1; step <= 5; ++step) {
                wait(16);
                t.move(0, root, 70, 160 - step * 8).commit();
            }
            wait(16);
            t.stationary(0).press(1, root, 390, 50).commit();
            wait(30);
            t.stationary(0).release(1, root, 390, 50).commit();
            wait(30);
            t.release(0, root, 70, 120).commit();
            wait(30);
            compare(button.clicks, 1, "button tapped while a fader is held");
        }

        function test_holdFaderTapMouseArea() {
            const t = touchEvent(root);
            t.press(0, root, 70, 160).commit();
            for (let step = 1; step <= 5; ++step) {
                wait(16);
                t.move(0, root, 70, 160 - step * 8).commit();
            }
            wait(16);
            t.stationary(0).press(1, root, 390, 180).commit();
            wait(30);
            t.stationary(0).release(1, root, 390, 180).commit();
            wait(30);
            t.release(0, root, 70, 120).commit();
            wait(30);
            compare(mouseArea.clicks, 1, "MouseArea tapped while a fader is held");
        }
    }
}
