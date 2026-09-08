pragma ComponentBehavior: Bound

import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import "Theme"

MenuBar {
    id: root

    // The application menu bar is stock Qt Quick Controls, so it arrives in the
    // Basic style: light grey bar, black text, white drop-downs. Every colour in
    // that style comes from the palette, so the chrome goes on as palette roles
    // rather than as replacement delegates -- which also leaves the mnemonics
    // ("&File" -> F underlined on Alt) working, since the contentItem is untouched.
    //
    // The palette does NOT reach the menus: a Menu is a Popup, and it takes the
    // default palette rather than its MenuBar's. Verified, not assumed -- a Menu
    // under a bar with palette.window "#2e2e2e" still reported "#efefef". Items
    // and separators DO inherit from their own Menu, so each menu carries the
    // palette and everything inside it follows. That is what ChromeMenu is for.
    palette.button: Theme.toolbarBackgroundColor
    palette.buttonText: Theme.deckTextColor
    // Basic paints a highlighted MenuBarItem with palette.mid.
    palette.mid: Qt.rgba(0.004, 0.863, 0.988, 0.18)
    palette.dark: Theme.midGray

    component ChromeMenu: Menu {
        // Matches the library context menus (Library/Track.qml): dark panel, grey
        // hairline, and the accent wash under the highlighted row.
        palette.window: Theme.darkGray2
        palette.windowText: Theme.deckTextColor
        palette.disabled.windowText: Theme.midGray
        palette.light: Qt.rgba(0.004, 0.863, 0.988, 0.18)
        palette.midlight: Qt.rgba(0.004, 0.863, 0.988, 0.28)
        palette.dark: Theme.midGray
        palette.mid: Theme.midGray
        palette.shadow: Theme.darkGray
    }

    required property ApplicationMenuActions actions
    property Menu developerMenu: null

    Component.onCompleted: {
        if (Mixxx.Application.developerMode) {
            developerMenu = developerMenuComponent.createObject(root);
            root.insertMenu(root.count - 1, developerMenu);
        }
    }

    ChromeMenu {
        title: qsTranslate("WMainMenuBar", "&File")

        MenuItem {
            action: root.actions.fileLoadDeck1
        }
        MenuItem {
            action: root.actions.fileLoadDeck2
        }
        MenuItem {
            action: root.actions.fileLoadDeck3
            enabled: root.actions.fileLoadDeck3.enabled
        }
        MenuItem {
            action: root.actions.fileLoadDeck4
            enabled: root.actions.fileLoadDeck4.enabled
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.fileQuit
        }
    }
    ChromeMenu {
        title: qsTranslate("WMainMenuBar", "&Library")

        MenuItem {
            action: root.actions.libraryRescan
        }
        MenuItem {
            action: root.actions.libraryExport
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.librarySearchCurrentView
        }
        MenuItem {
            action: root.actions.librarySearchTracks
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.libraryCreatePlaylist
        }
        MenuItem {
            action: root.actions.libraryCreateCrate
        }
    }
    ChromeMenu {
        title: qsTranslate("WMainMenuBar", "&View") + "\u200c"

        MenuItem {
            action: root.actions.viewShowMicrophone
        }
        MenuItem {
            action: root.actions.viewShowVinylControl
        }
        MenuItem {
            action: root.actions.viewShowPreviewDeck
        }
        MenuItem {
            action: root.actions.viewShowCoverArt
        }
        MenuItem {
            action: root.actions.viewShowKeywheel
        }
        MenuItem {
            action: root.actions.viewMaximizeLibrary
        }
        MenuItem {
            action: root.actions.viewShowEdgeSurface
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.viewShowAutoDJ
        }
        MenuItem {
            action: root.actions.viewFullScreen
        }
    }
    ChromeMenu {
        title: qsTranslate("WMainMenuBar", "&Options")

        ChromeMenu {
            enabled: Mixxx.Application.vinylControlAvailable
            title: qsTranslate("WMainMenuBar", "&Vinyl Control")

            MenuItem {
                action: root.actions.optionsEnableVinyl1
                enabled: root.actions.optionsEnableVinyl1.enabled
            }
            MenuItem {
                action: root.actions.optionsEnableVinyl2
                enabled: root.actions.optionsEnableVinyl2.enabled
            }
            MenuItem {
                action: root.actions.optionsEnableVinyl3
                enabled: root.actions.optionsEnableVinyl3.enabled
            }
            MenuItem {
                action: root.actions.optionsEnableVinyl4
                enabled: root.actions.optionsEnableVinyl4.enabled
            }
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.optionsRecordMix
        }
        MenuItem {
            action: root.actions.optionsEnableLiveBroadcasting
        }
        MenuItem {
            action: root.actions.optionsEnableKeyboardShortcuts
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.optionsPreferences
        }
        MenuItem {
            action: root.actions.optionsLegacyPreferences
        }
    }
    Component {
        id: developerMenuComponent

        ChromeMenu {
            title: qsTranslate("WMainMenuBar", "&Developer")

            MenuItem {
                action: root.actions.developerReloadSkin
            }
            MenuItem {
                action: root.actions.developerTools
            }
            MenuItem {
                action: root.actions.developerExperimentStats
            }
            MenuItem {
                action: root.actions.developerBaseStats
            }
            MenuItem {
                action: root.actions.developerDebugger
            }
        }
    }
    ChromeMenu {
        title: qsTranslate("WMainMenuBar", "&Help")

        MenuItem {
            action: root.actions.helpCommunitySupport
        }
        MenuItem {
            action: root.actions.helpUserManual
        }
        MenuItem {
            action: root.actions.helpKeyboardShortcuts
        }
        MenuItem {
            action: root.actions.helpSettingsDirectory
        }
        MenuItem {
            action: root.actions.helpTranslate
        }
        MenuSeparator {
        }
        MenuItem {
            action: root.actions.helpAbout
        }
    }
}
