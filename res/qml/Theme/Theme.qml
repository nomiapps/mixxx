pragma Singleton
import QtQuick 2.12

QtObject {
    property color accentColor: "#3a60be"
    property color backgroundColor: "#1e1e20"
    property color blue: "#01dcfc"
    property color bpmSliderBarColor: blue
    property color buttonActiveColor: white
    property color buttonDisableColor: lightGray
    property int buttonFontPixelSize: 10
    property color buttonNormalColor: midGray
    property color crossfaderBarColor: blue
    property color crossfaderOrientationColor: lightGray
    property color darkGray: "#0f0f0f"
    property color darkGray2: "#2e2e2e"
    property color darkGray3: "#3F3F3F"
    property color darkGray4: "#202020"
    property color deckActiveColor: white
    property color deckBackgroundColor: darkGray
    property color deckBeatjumpBackgroundColor: midGray3
    property color deckBeatjumpLabelColor: darkGray3
    property color deckEmptyCoverArt: darkGray3
    property color deckInfoBarBackgroundColor: '#0e0e0e'
    property color deckLineColor: darkGray2
    property color deckLoopBackgroundColor: midGray3
    property color deckLoopLabelColor: darkGray3
    property color deckTextColor: lightGray2
    property color effectColor: yellow
    property color effectUnitColor: red
    property color embeddedBackgroundColor: "#a0000000"
    property color eqFxColor: red
    property color eqHighColor: white
    property color eqLowColor: white
    property color eqMidColor: white
    property string fontFamily: "Open Sans"
    property color gainKnobColor: blue
    property color green: "#85c85b"
    // Resolved against THIS file (res/qml/Theme/) rather than left relative:
    // a bare "images/x.svg" is resolved by whichever component uses it, so
    // consumers in subdirectories (res/qml/Deck/...) looked for
    // res/qml/Deck/images/x.svg and got "QML BorderImage: Cannot open".
    property string imgBpmSliderBackground: Qt.resolvedUrl("../images/slider_bpm.svg")
    property string imgButton: Qt.resolvedUrl("../images/button.svg")
    property string imgButtonPressed: Qt.resolvedUrl("../images/button_pressed.svg")
    property string imgCrossfaderBackground: Qt.resolvedUrl("../images/slider_crossfader.svg")
    property string imgCrossfaderHandle: Qt.resolvedUrl("../images/slider_handle_crossfader.svg")
    property string imgKnob: Qt.resolvedUrl("../images/knob.svg")
    property string imgKnobMini: Qt.resolvedUrl("../images/miniknob.svg")
    property string imgKnobMiniShadow: Qt.resolvedUrl("../images/miniknob_shadow.svg")
    property string imgKnobShadow: Qt.resolvedUrl("../images/knob_shadow.svg")
    property string imgMicDuckingSlider: Qt.resolvedUrl("../images/slider_micducking.svg")
    property string imgMicDuckingSliderHandle: Qt.resolvedUrl("../images/slider_handle_micducking.svg")
    property string imgPopupBackground: imgButton
    property string imgSectionBackground: Qt.resolvedUrl("../images/section.svg")
    property string imgSliderHandle: Qt.resolvedUrl("../images/slider_handle.svg")
    property string imgVolumeSliderBackground: Qt.resolvedUrl("../images/slider_volume.svg")
    property color knobBackgroundColor: "#262626"
    property color libraryPanelSplitterBackground: "#1e1e1e"
    property color libraryPanelSplitterHandle: "#5f5f5f"
    property color libraryPanelSplitterHandleActive: "#7a7a7a"
    property color lightGray: "#747474"
    property color lightGray2: "#b0b0b0"
    property color lightGray3: "#939393"
    property color midGray: "#696969"
    property color midGray2: "#676767"
    property color midGray3: "#626262"
    property color panelSplitterBackground: backgroundColor
    property color panelSplitterHandle: midGray
    property color panelSplitterHandleActive: lightGray2
    property color pflActiveButtonColor: blue
    property color red: "#ea2a4e"
    property color samplerColor: blue
    property color sunkenBackgroundColor: "#0C0C0C"
    property color textColor: lightGray2
    property int textFontPixelSize: 14
    property color toolbarActiveColor: white
    property color toolbarBackgroundColor: darkGray2
    property color volumeSliderBarColor: blue
    property color warningColor: "#7D3B3B"
    property color waveformBeatColor: lightGray
    property color waveformCursorColor: white
    property color waveformMarkerDefault: '#ff7a01'
    property color waveformMarkerIntroOutroColor: '#2c5c9a'
    property color waveformMarkerLabel: Qt.rgba(255, 255, 255, 0.8)
    property color waveformMarkerLoopColor: '#00b400'
    property color waveformMarkerLoopColorDisabled: '#FFFFFF'
    property color waveformPostrollColor: midGray
    property color waveformPrerollColor: midGray
    property color white: "#D9D9D9"
    property color yellow: "#fca001"
    // Primitives named 2026-09-04. These four were loose literals: the first three
    // lived only in edge-layouts JSON (amber/purple/offWhite), and pureWhite was
    // written ~12 times with nothing to reference -- `white` is #D9D9D9, not white.
    property color amber: "#e0a040"
    property color purple: "#b060e0"
    property color offWhite: "#e8e8e8"
    property color pureWhite: "#ffffff"
    // The hairline that separates chrome panels. The deck/mixer restyle used
    // #303034 and #30343d for one concept; this names the dominant value so new
    // chrome stops adding another near-identical grey.
    property color panelBorderColor: "#303034"
    // Edge surface roles. Each DEFAULTS to the main-window token for the same
    // concept, so both surfaces match. This alias layer is the override point: give
    // any of these its own value (or a primitive like amber/purple) and the Edge
    // themes separately without touching the main window.
    property color edgeGainColor: gainKnobColor
    property color edgeLoopColor: deckActiveColor
    property color edgeEffectColor: effectColor
    property color edgeEffectUnitColor: effectUnitColor
    property color edgeQuickFxColor: eqFxColor
}
