import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Mixxx 1.0 as Mixxx
import "." as Setting
import "." as SettingComponents
import ".." as Skin
import "../Theme"

Category {
    id: root

    property bool dirty: false

    function load() {
        bpmEnabledInput.selected = Mixxx.Config.analyzerBpmEnabled ? "on" : "off";
        fixedTempoInput.selected = Mixxx.Config.analyzerBpmFixedTempo ? "on" : "off";
        fastAnalysisInput.selected = Mixxx.Config.analyzerBpmFastAnalysis ? "on" : "off";
        reanalyzeInput.selected = Mixxx.Config.analyzerBpmReanalyze ? "on" : "off";
        reanalyzeImportedInput.selected = Mixxx.Config.analyzerBpmReanalyzeImported ? "on" : "off";
        // The list is what THIS build has, not what the config remembers, so a
        // config naming a plugin that is gone selects nothing rather than
        // silently analysing with a different one.
        const plugins = Mixxx.Config.availableBeatPlugins();
        beatPluginInput.ids = plugins.map(p => p.id);
        beatPluginInput.model = plugins.map(p => p.name);
        beatPluginInput.currentIndex = beatPluginInput.ids.indexOf(Mixxx.Config.analyzerBeatPluginId);
        root.dirty = false;
    }
    function save() {
        Mixxx.Config.analyzerBpmEnabled = bpmEnabledInput.on;
        Mixxx.Config.analyzerBpmFixedTempo = fixedTempoInput.on;
        Mixxx.Config.analyzerBpmFastAnalysis = fastAnalysisInput.on;
        Mixxx.Config.analyzerBpmReanalyze = reanalyzeInput.on;
        Mixxx.Config.analyzerBpmReanalyzeImported = reanalyzeImportedInput.on;
        if (beatPluginInput.currentIndex >= 0)
            Mixxx.Config.analyzerBeatPluginId = beatPluginInput.ids[beatPluginInput.currentIndex];

        root.dirty = false;
    }

    label: qsTr("Analyzer")

    Component.onCompleted: root.load()

    ScrollView {
        anchors.bottom: parent.bottom
        // Anchored to the page rather than to buttonActions. They ARE siblings
        // here so the direct anchor would work, but this keeps one form across
        // the settings pages -- in Interface.qml the ScrollView sits inside a
        // tab, where buttonActions is an uncle, Qt refuses the anchor outright,
        // and the ScrollView silently grows to its content instead. clip is not
        // optional either: ScrollView does not clip by default, so without it
        // the content draws straight past the viewport.
        anchors.bottomMargin: buttonActions.height + 18
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 20
        clip: true
        spacing: 0

        ColumnLayout {
            spacing: 0
            width: root.width - 10

            RowLayout {
                Text {
                    Layout.bottomMargin: 14
                    Layout.leftMargin: 17
                    color: Theme.white
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    text: qsTr("Beat detection")
                }
            }
            Mixxx.SettingGroup {
                Layout.bottomMargin: 6
                Layout.fillWidth: true
                implicitHeight: bpmPane.implicitHeight + 20
                label: qsTr("Beat detection")

                Rectangle {
                    anchors.fill: parent
                    color: Theme.darkGray2

                    GridLayout {
                        id: bpmPane

                        anchors.bottomMargin: 10
                        anchors.fill: parent
                        anchors.leftMargin: 17
                        anchors.rightMargin: 17
                        anchors.topMargin: 10
                        columnSpacing: 20
                        columns: 2
                        rowSpacing: 15

                        RowLayout {
                            Layout.preferredWidth: (bpmPane.width - bpmPane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText1.implicitHeight
                                keywords: ["bpm", "beat", "tempo", "beatgrid"]
                                label: qsTr("Detect BPM and beatgrid")

                                Text {
                                    id: labelText1

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            RatioChoice {
                                id: bpmEnabledInput

                                readonly property bool on: selected == "on"

                                inactiveColor: Theme.darkGray4
                                options: ["on", "off"]

                                onSelectedChanged: root.dirty = true
                            }
                        }
                        RowLayout {
                            Layout.preferredWidth: (bpmPane.width - bpmPane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText2.implicitHeight
                                keywords: ["plugin", "vamp", "analyser", "queen mary", "soundtouch"]
                                label: qsTr("Beat analyser")

                                Text {
                                    id: labelText2

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Skin.ComboBox {
                                id: beatPluginInput

                                property var ids: []

                                Layout.preferredWidth: (bpmPane.width - bpmPane.columnSpacing) * 0.4
                                enabled: bpmEnabledInput.on
                                model: []
                                opacity: enabled ? 1 : 0.5

                                onCurrentIndexChanged: root.dirty = true
                            }
                        }
                        RowLayout {
                            Layout.preferredWidth: (bpmPane.width - bpmPane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText3.implicitHeight
                                keywords: ["tempo", "constant", "variable"]
                                label: qsTr("Assume a constant tempo")

                                Text {
                                    id: labelText3

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            RatioChoice {
                                id: fixedTempoInput

                                readonly property bool on: selected == "on"

                                enabled: bpmEnabledInput.on
                                inactiveColor: Theme.darkGray4
                                opacity: enabled ? 1 : 0.5
                                options: ["on", "off"]

                                onSelectedChanged: root.dirty = true
                            }
                        }
                        RowLayout {
                            Layout.preferredWidth: (bpmPane.width - bpmPane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText4.implicitHeight
                                keywords: ["fast", "quick", "import"]
                                label: qsTr("Fast analysis")

                                Text {
                                    id: labelText4

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            RatioChoice {
                                id: fastAnalysisInput

                                readonly property bool on: selected == "on"

                                enabled: bpmEnabledInput.on
                                inactiveColor: Theme.darkGray4
                                opacity: enabled ? 1 : 0.5
                                options: ["on", "off"]

                                onSelectedChanged: root.dirty = true
                            }
                        }
                    }
                }
            }
            Text {
                Layout.leftMargin: 17
                Layout.preferredWidth: root.width - 44
                Layout.topMargin: 8
                color: Theme.deckTextColor
                font.pixelSize: 12
                text: qsTr("Fast analysis reads only the first part of each track. Much quicker over a large import, and wrong on anything whose tempo moves.")
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Text {
                    Layout.bottomMargin: 14
                    Layout.leftMargin: 17
                    Layout.topMargin: 24
                    color: Theme.white
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    text: qsTr("Re-analysis")
                }
            }
            Mixxx.SettingGroup {
                Layout.bottomMargin: 6
                Layout.fillWidth: true
                implicitHeight: reanalyzePane.implicitHeight + 20
                label: qsTr("Re-analysis")

                Rectangle {
                    anchors.fill: parent
                    color: Theme.darkGray2

                    GridLayout {
                        id: reanalyzePane

                        anchors.bottomMargin: 10
                        anchors.fill: parent
                        anchors.leftMargin: 17
                        anchors.rightMargin: 17
                        anchors.topMargin: 10
                        columnSpacing: 20
                        columns: 2
                        rowSpacing: 15

                        RowLayout {
                            Layout.preferredWidth: (reanalyzePane.width - reanalyzePane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText5.implicitHeight
                                keywords: ["reanalyse", "reanalyze", "settings"]
                                label: qsTr("Re-analyse when these settings change")

                                Text {
                                    id: labelText5

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            RatioChoice {
                                id: reanalyzeInput

                                readonly property bool on: selected == "on"

                                inactiveColor: Theme.darkGray4
                                options: ["on", "off"]

                                onSelectedChanged: root.dirty = true
                            }
                        }
                        RowLayout {
                            Layout.preferredWidth: (reanalyzePane.width - reanalyzePane.columnSpacing) / 2

                            Mixxx.SettingParameter {
                                Layout.fillWidth: true
                                // SettingParameter is a bare QQuickItem: implicit size 0. Without
                                // this the label has no height and paints nothing at all.
                                implicitHeight: labelText6.implicitHeight
                                keywords: ["imported", "beatgrid", "serato", "rekordbox", "traktor"]
                                label: qsTr("Re-analyse imported beatgrids")

                                Text {
                                    id: labelText6

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.white
                                    elide: Text.ElideRight
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignLeft
                                    text: parent.label
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            RatioChoice {
                                id: reanalyzeImportedInput

                                readonly property bool on: selected == "on"

                                inactiveColor: Theme.darkGray4
                                options: ["on", "off"]

                                onSelectedChanged: root.dirty = true
                            }
                        }
                    }
                }
            }
            Text {
                Layout.leftMargin: 17
                Layout.preferredWidth: root.width - 44
                Layout.topMargin: 8
                color: Theme.deckTextColor
                font.pixelSize: 12
                text: qsTr("Imported beatgrids come from other DJ software. Re-analysing replaces them with Mixxx's own.")
                wrapMode: Text.WordWrap
            }
        }
    }
    Item {
        id: buttonActions

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: parent.right
        anchors.rightMargin: 14
        height: 20

        SettingComponents.FormButton {
            anchors.left: parent.left
            backgroundColor: Theme.warningColor
            enabled: root.dirty
            opacity: enabled ? 1.0 : 0.5
            text: qsTr("Reset")

            onClicked: root.load()
        }
        SettingComponents.FormButton {
            id: saveButton

            anchors.right: parent.right
            enabled: root.dirty
            opacity: enabled ? 1.0 : 0.5
            text: qsTr("Save")

            onClicked: root.save()
        }
        SettingComponents.FormButton {
            anchors.right: saveButton.left
            anchors.rightMargin: 10
            enabled: root.dirty
            opacity: enabled ? 1.0 : 0.5
            text: qsTr("Cancel")

            onClicked: root.load()
        }
    }
}
