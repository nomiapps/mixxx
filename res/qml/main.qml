import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

ApplicationWindow {
    id: root

    property alias show4decks: show4DecksButton.checked
    property alias showEffects: showEffectsButton.checked
    property alias showSamplers: showSamplersButton.checked
    property alias maximizeLibrary: maximizeLibraryButton.checked

    width: 1920
    height: 1080
    color: Theme.backgroundColor
    visible: true

    // Qt cascades a new window near the top-left of its screen and, on a
    // multi-monitor desktop, can leave the title bar above the screen edge
    // (observed at y = -45 on a 4K primary) so it can't be grabbed. Geometry
    // isn't persisted between launches, so rather than merely clamping the
    // cascade position, place the window deterministically: centred on its
    // screen, then clamped so the title bar is always reachable. Runs
    // deferred -- at Component.onCompleted the window manager hasn't
    // assigned the real screen/position yet.
    function clampOntoScreen() {
        // root.screen can still be null when this runs (observed: every pass
        // early-returned and Qt's cascade position stuck). Fall back to the
        // primary (virtual origin 0,0), then to the first screen.
        let s = root.screen;
        if (!s) {
            const all = Qt.application.screens;
            for (let i = 0; i < all.length; ++i) {
                if (all[i].virtualX === 0 && all[i].virtualY === 0) {
                    s = all[i];
                    break;
                }
            }
            if (!s && all.length > 0)
                s = all[0];
        }
        if (!s)
            return ;

        // NOTE: Screen.desktopAvailableWidth/Height is the size of the whole
        // VIRTUAL desktop (all monitors), not this screen. Using it here
        // centred the window on the multi-monitor union, landing it in the
        // primary's bottom-right corner (observed: 5120x2880 "available" on a
        // 2560x1440-logical screen). Screen.width/height is this screen only.
        const sw = s.width;
        const sh = s.height;

        root.width = Math.min(root.width, sw);
        root.height = Math.min(root.height, sh);

        // Centre on this screen.
        root.x = s.virtualX + Math.round((sw - root.width) / 2);
        root.y = s.virtualY + Math.round((sh - root.height) / 2);

        // Clamp inside this screen so the title bar is always reachable, even
        // if the window is as large as the screen.
        root.x = Math.min(Math.max(root.x, s.virtualX), s.virtualX + sw - root.width);
        root.y = Math.min(Math.max(root.y, s.virtualY), s.virtualY + sh - root.height);
    }

    Component.onCompleted: clampTimer.start()

    // Qt assigns the final screen and applies its own initial (cascade)
    // placement asynchronously, and on a multi-monitor mixed-DPI desktop that
    // can land AFTER a single early pass -- observed overwriting a 250ms
    // placement, leaving the window in the primary's bottom-right corner.
    // So re-place when the screen changes, and repeat the deferred pass a few
    // times over the first ~1.2s. Placement is deterministic (centre + clamp),
    // so repeated passes converge and the last one wins.
    onScreenChanged: root.clampOntoScreen()

    Timer {
        id: clampTimer

        property int passes: 0

        interval: 400
        repeat: true
        onTriggered: {
            root.clampOntoScreen();
            if (++passes >= 3)
                stop();
        }
    }

    Column {
        anchors.fill: parent

        Rectangle {
            id: toolbar

            width: parent.width
            height: 36
            color: Theme.toolbarBackgroundColor
            radius: 1

            Row {
                padding: 5
                spacing: 5

                Skin.Button {
                    id: show4DecksButton

                    text: "4 Decks"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: maximizeLibraryButton

                    text: "Library"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showEffectsButton

                    text: "Effects"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showSamplersButton

                    text: "Sampler"
                    activeColor: Theme.white
                    checkable: true
                }

                Skin.Button {
                    id: showPreferencesButton

                    text: "Prefs"
                    activeColor: Theme.white
                    onClicked: {
                        Mixxx.PreferencesDialog.show();
                    }
                }

                Skin.Button {
                    id: showDevToolsButton

                    text: "Develop"
                    activeColor: Theme.white
                    checkable: true
                    checked: devToolsWindow.visible
                    onClicked: {
                        if (devToolsWindow.visible)
                            devToolsWindow.close();
                        else
                            devToolsWindow.show();
                    }

                    DeveloperToolsWindow {
                        id: devToolsWindow

                        width: 640
                        height: 480
                    }
                }
            }
        }

        Skin.WaveformDisplay {
            id: deck3waveform

            group: "[Channel3]"
            width: root.width
            height: 120
            visible: root.show4decks && !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck3waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck1waveform

            group: "[Channel1]"
            width: root.width
            height: 120
            visible: !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck1waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck2waveform

            group: "[Channel2]"
            width: root.width
            height: 120
            visible: !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck2waveform
            }
        }

        Skin.WaveformDisplay {
            id: deck4waveform

            group: "[Channel4]"
            width: root.width
            height: 120
            visible: root.show4decks && !root.maximizeLibrary

            FadeBehavior on visible {
                fadeTarget: deck4waveform
            }
        }

        Skin.DeckRow {
            id: decks12

            leftDeckGroup: "[Channel1]"
            rightDeckGroup: "[Channel2]"
            width: parent.width
            minimized: root.maximizeLibrary
        }

        Skin.CrossfaderRow {
            id: crossfader

            crossfaderWidth: decks12.mixer.width
            width: parent.width
            visible: !root.maximizeLibrary

            Skin.FadeBehavior on visible {
                fadeTarget: crossfader
            }
        }

        Skin.DeckRow {
            id: decks34

            leftDeckGroup: "[Channel3]"
            rightDeckGroup: "[Channel4]"
            width: parent.width
            minimized: root.maximizeLibrary
            visible: root.show4decks

            Skin.FadeBehavior on visible {
                fadeTarget: decks34
            }
        }

        Skin.SamplerRow {
            id: samplers

            width: parent.width
            visible: root.showSamplers

            Skin.FadeBehavior on visible {
                fadeTarget: samplers
            }
        }

        Skin.EffectRow {
            id: effects

            width: parent.width
            visible: root.showEffects

            Skin.FadeBehavior on visible {
                fadeTarget: effects
            }
        }

        Skin.Library {
            width: parent.width
            height: parent.height - y
        }

        move: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: 150
            }
        }
    }
}
