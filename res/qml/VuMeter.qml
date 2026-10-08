import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Window
import "Theme"

Rectangle {
    id: root

    required property string group
    required property string key
    // Sampled once per frame of the window this meter is in, instead of bound
    // to the control. A binding requests a render on every engine callback,
    // and a request that lands mid-frame parks the GUI thread until that
    // window's render thread is free: on the 60 Hz Edge panel, most of a
    // frame period, dozens of times a second. Not a FrameAnimation either:
    // that ticks from the global animation driver, which every window's frame
    // loop advances, so a 240 Hz main window drove it 240 times a second.
    // afterFrameEnd is the window's own render loop, delivered queued on the
    // GUI thread right after its frame ended, while its render thread is idle.
    property real level: 0

    color: "black"
    radius: width / 2

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }
    Connections {
        function onAfterFrameEnd() {
            const v = control.parameter;
            if (v !== root.level)
                root.level = v;
        }

        target: root.Window.window
    }
    // The lit part of the meter: the full-height gradient, seen through a
    // window as tall as the level. This was the gradient under a mask, a
    // shader effect per meter, redrawn at meter rate.
    Item {
        id: lit

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 1
        anchors.right: parent.right
        clip: true
        height: root.level * (root.height - 2 * anchors.margins)

        Rectangle {
            antialiasing: false // for performance reasons
            height: root.height
            // Rounded like the meter itself, so the lit part follows its ends.
            radius: width / 2
            width: root.width
            // Placed over the whole meter, whatever the window shows of it.
            x: -lit.x
            y: -lit.y

            gradient: Gradient {
                GradientStop {
                    color: Theme.red
                    position: 0.1
                }
                GradientStop {
                    color: Theme.yellow
                    position: 0.15
                }
                GradientStop {
                    color: Theme.yellow
                    position: 0.25
                }
                GradientStop {
                    color: Theme.green
                    position: 0.3
                }
                GradientStop {
                    color: Theme.green
                    position: 1
                }
            }
        }
    }
}
