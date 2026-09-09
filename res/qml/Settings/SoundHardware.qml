import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

Category {
    id: root

    // Loudest the main mix and the tone channel got during the last test, held
    // after it ends so the reading survives being read. -1 means "not measured
    // yet"; 0 means measured and silent, which is the interesting answer.
    property real audioTestMainPeak: -1
    // The synth routing to put back when the test tone ends, and which output
    // is being tested ("" when nothing is sounding).
    property var audioTestSaved: null
    // Which output the FINISHED test used, so the reading afterwards can be
    // specific -- audioTestTarget is cleared the moment the tone stops.
    property string audioTestLast: ""
    property string audioTestTarget: ""
    property real audioTestTonePeak: -1
    property bool committing: false
    // Set once Save has been refused for having no output, so a second press
    // goes through: emptying the routing on purpose is rare but legitimate.
    property bool emptyOutputConfirmed: false
    // The engine's real rate against the one configured. Both are only
    // meaningful once a device is open, hence the > 0 test.
    readonly property bool engineSampleRateDiffers: appSampleRate.value > 0 && sampleRate.selected && Math.round(appSampleRate.value) !== parseInt(sampleRate.selected)
    property bool hasChanges: router.hasChanges

    function load() {
        const manager = Mixxx.SoundManager;
        mainMixEnabled.selected = mainMixEnabled.options[mainEnabled.value ? 0 : 1];
        mainOutputMode.selected = mainOutputMode.options[monoMix.value ? 0 : 1];
        soundClock.selected = soundClock.options[manager.getForceNetworkClock() ? 1 : 0];
        sampleRate.update(manager.getAPI());
        sampleRate.selected = qsTr("%1 Hz").arg(manager.getSampleRate());
        audioBuffer.currentIndex = manager.getAudioBufferSizeIndex() - 1;
        microphoneMonitorMode.enabled = manager.hasMicInputs();
        microphoneMonitorMode.currentIndex = micMonitorMode.value;
        soundApi.options = manager.getHostAPIList();
        soundApi.selected = manager.getAPI();
        keylock.update();
        keylock.selected = keylock.options[manager.getKeylockEngine()];

        // Router
        router.multiSoundcard.selected = router.multiSoundcard.options[manager.getSyncBuffers()];
        router.update(manager.getAPI());

        //Delays
        mainDelayLabel.enabled = mainEnabled.value;
        mainDelaySlider.enabled = mainEnabled.value;
        mainDelaySlider.value = mainDelay.value;
        boothDelayLabel.enabled = boothEnabled.value;
        boothDelaySlider.enabled = boothEnabled.value;
        boothDelaySlider.value = boothDelay.value;
        headphoneDelayLabel.enabled = headEnabled.value;
        headphoneDelaySlider.enabled = headEnabled.value;
        headphoneDelaySlider.value = headDelay.value;

        root.hasChanges = Qt.binding(function () {
            return router.hasChanges;
        });
        // Re-arm the no-output warning: load() runs after a commit and after
        // Cancel, so each fresh round of edits gets warned about once.
        root.emptyOutputConfirmed = false;
    }
    // Output connections currently drawn in the router, counted the same way
    // save() serialises them.
    function connectedOutputCount() {
        let count = 0;
        for (let device of Object.keys(router.outputs)) {
            for (let address of Object.keys(router.outputs[device].gateways)) {
                const gateway = router.outputs[device].gateways[address];
                const connections = gateway.node && gateway.node.assignedEdges ? gateway.node.assignedEdges() : {};
                count += Object.keys(connections).length;
            }
        }
        return count;
    }
    function save() {
        const manager = Mixxx.SoundManager;
        // Saving a routing with nothing connected leaves Mixxx silent, and the
        // page used to accept it without a word. It is easy to arrive at by
        // accident: changing the Sound API rebuilds the device list, which
        // drops the connections that belonged to the old API, so an API change
        // followed by Save is enough. Legacy warns about this (noOutputDlg,
        // "Mixxx will barely work with no outs"); the New UI said nothing and
        // wrote a config with no device in it -- which, until the startup
        // dialog landed, also meant the next launch died. Refuse once, with a
        // reason, and let a second press through: an empty routing is rare but
        // it is allowed.
        if (root.connectedOutputCount() === 0 && !root.emptyOutputConfirmed) {
            root.emptyOutputConfirmed = true;
            errorMessage.text = "No output connected -- Mixxx would be silent. Connect the mixer's Main to a device, or press Save again to save it anyway.";
            return;
        }
        root.emptyOutputConfirmed = false;
        mainEnabled.value = mainMixEnabled.options.indexOf(mainMixEnabled.selected);
        monoMix.value = !mainOutputMode.options.indexOf(mainOutputMode.selected);
        manager.setForceNetworkClock(soundClock.options[1] == soundClock.selected);
        manager.setSampleRate(parseInt(sampleRate.selected));
        manager.setAudioBufferSizeIndex(audioBuffer.currentIndex + 1);
        micMonitorMode.value = microphoneMonitorMode.currentIndex;
        manager.setAPI(soundApi.selected);
        manager.setKeylockEngine(keylock.options.indexOf(keylock.selected));

        // Router
        manager.setSyncBuffers(router.multiSoundcard.options.indexOf(router.multiSoundcard.selected));

        let connectionsHandler = (connections, device) => {
            for (let channel of Object.keys(connections)) {
                let connection = connections[channel];
                let type;
                let index = 0;
                let isOutput = true;
                if (connection.source.entity.name == "Mixer") {
                    switch (connection.source.address) {
                    case "Main":
                        type = 0;
                        break;
                    case "PFL":
                        type = 1;
                        break;
                    case "Booth":
                        type = 2;
                        break;
                    case "Left Bus":
                    case "Center Bus":
                    case "Right Bus":
                        index = connection.source.address.startsWith("Left") ? 0 : connection.source.address.startsWith("Right") ? 2 : 1;
                        type = 3;
                        break;
                    default:
                        console.error(`unsupported address: ${connection.source.address}`);
                        continue;
                    }
                } else if (connection.sink.entity.name == "Mixer") {
                    isOutput = false;
                    type = connection.sink.address == "Auxiliary" ? 7 : 6;
                    index = connection.sink.instance;
                } else if (connection.source.entity.name.startsWith("Deck ") && connection.source.address == "Output") {
                    type = 4;
                    index = parseInt(connection.source.entity.name.split(' ')[1]) - 1;
                } else if (connection.sink.entity.name.startsWith("Deck ")) {
                    isOutput = false;
                    type = connection.source.address == "Output" ? 4 : 5;
                    index = parseInt(connection.sink.entity.name.split(' ')[1]) - 1;
                } else if (connection.sink.entity.name == "Microphone") {
                    isOutput = false;
                    type = 6;
                    index = connection.sink.instance;
                } else if (connection.sink.entity.name == "Auxiliary") {
                    isOutput = false;
                    type = 7;
                    index = connection.sink.instance;
                } else if (connection.sink.entity.name == "RecordBroadcast") {
                    isOutput = false;
                    type = 8;
                    index = connection.sink.instance;
                } else {
                    console.error(`unsupported entity: ${connection.source.entity.name} ${connection.sink.entity.name}`);
                    continue;
                }
                console.log(isOutput ? "addOutput" : "addInput", device, type, channel * 2, index);
                if (isOutput) {
                    manager.addOutput(device, type, channel * 2, index);
                } else {
                    manager.addInput(device, type, channel * 2, index);
                }
            }
        };
        manager.clearOutputs();
        for (let device of Object.keys(router.outputs)) {
            for (let address of Object.keys(router.outputs[device].gateways)) {
                let gateway = router.outputs[device].gateways[address];
                let connections = gateway.node && gateway.node.assignedEdges ? gateway.node.assignedEdges() : {};
                connectionsHandler(connections, gateway.device);
            }
        }
        manager.clearInputs();
        for (let device of Object.keys(router.inputs)) {
            for (let address of Object.keys(router.inputs[device].gateways)) {
                let gateway = router.inputs[device].gateways[address];
                let connections = gateway.node && gateway.node.assignedEdges ? gateway.node.assignedEdges() : {};
                connectionsHandler(connections, gateway.device);
            }
        }

        mainDelay.value = mainDelaySlider.value;
        boothDelay.value = boothDelaySlider.value;
        headDelay.value = headphoneDelaySlider.value;

        root.committing = true;
        manager.commit();
    }

    // Plays a tone so you can hear whether an output is actually working, and
    // which one you are hearing. Mixxx has no test-tone generator, but the
    // fork's synth channel is one: [Synth1] exists from startup (coreservices,
    // kSynthCount) and `note_on` takes a MIDI note number. Which output hears
    // it is that channel's own routing -- `main_mix` reaches the main output,
    // and the booth output carries the same mix through its own gain, so a
    // booth test is the main tone with a reminder of that; `pfl` reaches the
    // headphones and is PRE-fader, so the headphone test can silence the main
    // mix and still sound. The routing found is put back when the tone ends,
    // including if the page is left mid-tone.
    function startAudioTest(target) {
        if (root.audioTestTarget !== "") {
            root.stopAudioTest();
        }
        root.audioTestSaved = {
            "mainMix": synthMainMix.value,
            "pfl": synthPfl.value,
            "volume": synthVolume.value,
            "mute": synthMute.value
        };
        root.audioTestTarget = target;
        root.audioTestLast = target;
        root.audioTestTonePeak = 0;
        root.audioTestMainPeak = 0;
        synthMute.value = 0;
        synthVolume.value = 1;
        synthMainMix.value = target === "headphones" ? 0 : 1;
        synthPfl.value = target === "headphones" ? 1 : 0;
        // A4. An integer note is full velocity (the fractional part is where
        // velocity would go), and 440 Hz is high enough to be unmistakable on a
        // laptop speaker and low enough not to be shrill on monitors.
        synthNoteOn.value = 69;
        audioTestTimer.restart();
    }
    // What the two meters mean, once a test has finished. The ear cannot tell
    // "no tone was made" from "a tone was made and never left the building",
    // and that is exactly the difference between a Mixxx problem and a device
    // or cabling one.
    function audioTestReading() {
        if (root.audioTestTonePeak < 0) {
            return "Nothing sounding.";
        }
        if (root.audioTestTonePeak === 0) {
            return "Nothing sounding. The tone channel never registered a level, so the tone was not generated -- this is a Mixxx problem, not a wiring one.";
        }
        if (root.audioTestLast === "headphones") {
            return "Nothing sounding. The tone was generated and cued to the headphone output. If you heard nothing, the fault is that output or its device, not the mix.";
        }
        if (root.audioTestMainPeak === 0) {
            return "Nothing sounding. The tone was generated but never reached the main mix -- something is muting or unrouting it before the outputs.";
        }
        return "Nothing sounding. The tone reached the main mix, so Mixxx made the sound and handed it over: if you heard nothing, the fault is downstream -- the device selected in the routing below, its own volume, or the cable.";
    }
    function stopAudioTest() {
        audioTestTimer.stop();
        synthNoteOff.value = 69;
        // The note above may still be in its release; this cuts it either way.
        synthAllNotesOff.value = 1;
        synthAllNotesOff.value = 0;
        if (root.audioTestSaved) {
            synthMainMix.value = root.audioTestSaved.mainMix;
            synthPfl.value = root.audioTestSaved.pfl;
            synthVolume.value = root.audioTestSaved.volume;
            synthMute.value = root.audioTestSaved.mute;
            root.audioTestSaved = null;
        }
        root.audioTestTarget = "";
    }

    label: "Sound hardware"
    tabs: ["engine", "delays", "stats", "audio test"]

    Component.onCompleted: {
        load();
    }
    // Applying a new configuration closes and reopens the devices under the
    // tone, and leaving the page would strand the synth muted or cued.
    onDeactivated: {
        if (root.audioTestTarget !== "") {
            root.stopAudioTest();
        }
    }

    Mixxx.ControlProxy {
        id: mainEnabled

        group: "[Master]"
        key: "enabled"
    }
    Mixxx.ControlProxy {
        id: headEnabled

        group: "[Master]"
        key: "headEnabled"
    }
    Mixxx.ControlProxy {
        id: boothEnabled

        group: "[Master]"
        key: "booth_enabled"
    }
    Mixxx.ControlProxy {
        id: mainDelay

        group: "[Master]"
        key: "delay"
    }
    Mixxx.ControlProxy {
        id: headDelay

        group: "[Master]"
        key: "headDelay"
    }
    Mixxx.ControlProxy {
        id: boothDelay

        group: "[Master]"
        key: "boothDelay"
    }
    Mixxx.ControlProxy {
        id: monoMix

        group: "[Master]"
        key: "mono_mixdown"
    }
    Mixxx.ControlProxy {
        id: micMonitorMode

        group: "[Master]"
        key: "talkover_mix"
    }
    Mixxx.ControlProxy {
        id: appSampleRate

        group: "[App]"
        key: "samplerate"
    }
    Mixxx.ControlProxy {
        id: synthNoteOn

        group: "[Synth1]"
        key: "note_on"
    }
    Mixxx.ControlProxy {
        id: synthNoteOff

        group: "[Synth1]"
        key: "note_off"
    }
    Mixxx.ControlProxy {
        id: synthAllNotesOff

        group: "[Synth1]"
        key: "all_notes_off"
    }
    Mixxx.ControlProxy {
        id: synthMainMix

        group: "[Synth1]"
        key: "main_mix"
    }
    Mixxx.ControlProxy {
        id: synthPfl

        group: "[Synth1]"
        key: "pfl"
    }
    Mixxx.ControlProxy {
        id: synthVolume

        group: "[Synth1]"
        key: "volume"
    }
    Mixxx.ControlProxy {
        id: synthMute

        group: "[Synth1]"
        key: "mute"
    }
    // The two places the tone can be measured, so the test can say WHERE it
    // stopped rather than only that you did or did not hear something. The
    // synth channel's own meter proves the tone was generated; the main mix
    // meter proves it reached the mix. (There is no headphone meter in the
    // engine -- EngineMixer builds one EngineVuMeter, for [Main] -- so the
    // headphone test relies on the tone meter, and the main mix reading 0 for
    // it is correct: main_mix is 0 during that test.)
    Mixxx.ControlProxy {
        id: synthVu

        group: "[Synth1]"
        key: "vu_meter"

        onValueChanged: {
            if (root.audioTestTarget !== "") {
                root.audioTestTonePeak = Math.max(root.audioTestTonePeak, synthVu.value);
            }
        }
    }
    Mixxx.ControlProxy {
        id: mainVu

        group: "[Main]"
        key: "vu_meter"

        onValueChanged: {
            if (root.audioTestTarget !== "") {
                root.audioTestMainPeak = Math.max(root.audioTestMainPeak, mainVu.value);
            }
        }
    }
    Timer {
        id: audioTestTimer

        interval: 1500

        onTriggered: {
            root.stopAudioTest();
        }
    }
    ScrollView {
        id: scrollView

        anchors.fill: parent

        ColumnLayout {
            Item {
                id: tabSection

                Layout.fillWidth: true
                // Stats has no measurable content of its own, so it borrows the
                // delays height the way it always has.
                Layout.preferredHeight: root.selectedIndex == 0 ? engine.height : (root.selectedIndex == 3 ? audioTest.height : delays.height)

                Mixxx.SettingGroup {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    label: "Engine"
                    visible: root.selectedIndex == 0

                    onActivated: {
                        root.selectedIndex = 0;
                    }

                    RowLayout {
                        id: engine

                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: tabSection.width * 0.04

                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop
                            Layout.fillWidth: true

                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Main Mix"

                                    Mixxx.SettingParameter {
                                        label: "Main Mix"
                                    }
                                }
                                RatioChoice {
                                    id: mainMixEnabled

                                    options: ["on", "off"]
                                    selected: options[mainEnabled.value ? 0 : 1]

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Main Output Mode"

                                    Mixxx.SettingParameter {
                                        label: "Main Output Mode"
                                    }
                                }
                                RatioChoice {
                                    id: mainOutputMode

                                    maxWidth: tabSection.width * 0.18
                                    options: ["mono", "stereo"]
                                    selected: options[monoMix.value ? 0 : 1]

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Sound Clock"

                                    Mixxx.SettingParameter {
                                        label: "Sound Clock"
                                    }
                                }
                                RatioChoice {
                                    id: soundClock

                                    maxWidth: tabSection.width * 0.28
                                    options: ["soundcard", "network"]

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Keylock engine"
                                }
                                RatioChoice {
                                    id: keylock

                                    function update() {
                                        let options = [];
                                        let tooltips = [];
                                        for (let engine of Mixxx.SoundManager.getKeylockEngines()) {
                                            switch (engine) {
                                            case 0:
                                                options.push(qsTr("Soundtouch"));
                                                tooltips.push(qsTr("Faster"));
                                                break;
                                            case 1:
                                                options.push(qsTr("Rubberband"));
                                                tooltips.push(qsTr("Better"));
                                                break;
                                            case 2:
                                                options.push(qsTr("Rubberband R3"));
                                                tooltips.push(qsTr("Near-hi-fi quality"));
                                                break;
                                            }
                                        }
                                        keylock.options = options;
                                        keylock.tooltips = tooltips;
                                    }

                                    maxWidth: tabSection.width * 0.4
                                    normalizedWidth: false
                                    options: []
                                    tooltips: []

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                    }

                                    Mixxx.SettingParameter {
                                        label: "Keylock engine"
                                    }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop

                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Sound API"
                                }
                                RatioChoice {
                                    id: soundApi

                                    maxWidth: tabSection.width * 0.4
                                    options: []

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                        router.update(soundApi.selected);
                                        sampleRate.update(soundApi.selected);
                                    }

                                    Mixxx.SettingParameter {
                                        label: "Sound API"
                                    }
                                }
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: root.engineSampleRateDiffers ? Theme.amber : Theme.white
                                    font.pixelSize: 14
                                    // What is chosen and what the engine ended
                                    // up running at are two different numbers,
                                    // and only the first was ever shown. A
                                    // device can refuse the rate and impose its
                                    // own -- ASIO4ALL answered 44100 to a
                                    // config asking for 48000 -- and nothing
                                    // said so, which is how you end up chasing a
                                    // 44.1/48 mismatch you cannot see. [App]
                                    // samplerate is what the engine actually
                                    // runs at, so say it when it disagrees.
                                    text: root.engineSampleRateDiffers ? "Sample Rate  (running at " + Math.round(appSampleRate.value) + " Hz)" : "Sample Rate"

                                    Mixxx.SettingParameter {
                                        label: "Sample Rate"
                                    }
                                }
                                RatioChoice {
                                    id: sampleRate

                                    function update(api) {
                                        let data = [];
                                        for (let sampleRate of Mixxx.SoundManager.getSampleRates(api)) {
                                            data.push(qsTr("%1 Hz").arg(sampleRate));
                                        }
                                        sampleRate.options = data;
                                    }

                                    Layout.minimumWidth: sampleRate.implicitWidth
                                    maxWidth: tabSection.width * 0.34
                                    options: []

                                    onSelectedChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                            Connections {
                                function onSelectedChanged() {
                                    let sampleRateValue = parseInt(sampleRate.selected);
                                    audioBuffer.update(sampleRateValue);
                                }

                                target: sampleRate
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    text: "Audio Buffer"

                                    Mixxx.SettingParameter {
                                        label: "Audio Buffer"
                                    }
                                }
                                Skin.ComboBox {
                                    id: audioBuffer

                                    function update(sampleRate) {
                                        let data = [];
                                        let framesPerBuffer = 1;
                                        for (; framesPerBuffer / sampleRate * 1000 < 1.0; framesPerBuffer *= 2) {}
                                        for (let i = 0; i < 7; i++) {
                                            const latency = framesPerBuffer / sampleRate * 1000;
                                            // i + 1 in the next line is a latency index as described in SSConfig
                                            data.push(qsTr("%1 ms").arg(latency.toFixed(1)));
                                            framesPerBuffer *= 2;
                                        }
                                        let currentIndex = audioBuffer.currentIndex;
                                        audioBuffer.model = data;
                                        audioBuffer.currentIndex = currentIndex;
                                    }

                                    clip: true
                                    font.pixelSize: 12
                                    spacing: 2

                                    onCurrentIndexChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                            RowLayout {
                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.white
                                    font.pixelSize: 14
                                    opacity: Mixxx.SoundManager.hasMicInputs() ? 1.0 : 0.5
                                    text: "Microphone Monitor Mode"

                                    Mixxx.SettingParameter {
                                        label: "Microphone Monitor Mode"
                                    }
                                }
                                Skin.ComboBox {
                                    id: microphoneMonitorMode

                                    clip: true
                                    font.pixelSize: 12
                                    model: ["Main output only", "Main and booth outputs", "Direct monitor (recording and broadcasting only)"]
                                    opacity: enabled ? 1.0 : 0.5
                                    spacing: 2

                                    onCurrentIndexChanged: {
                                        root.hasChanges = true;
                                    }
                                }
                            }
                        }
                    }
                }
                Mixxx.SettingGroup {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    label: "Delays"
                    visible: root.selectedIndex == 1

                    onActivated: {
                        root.selectedIndex = 1;
                    }

                    GridLayout {
                        id: delays

                        anchors.left: parent.left
                        anchors.right: parent.right
                        columns: 2
                        rowSpacing: 0

                        Text {
                            id: mainDelayLabel

                            Layout.fillWidth: true
                            color: Theme.white
                            font.pixelSize: 14
                            opacity: enabled ? 1 : 0.5
                            text: "Main Output"

                            Mixxx.SettingParameter {
                                label: "Main Output"
                            }
                        }
                        Skin.Slider {
                            id: mainDelaySlider

                            Layout.fillWidth: true
                            markers: ["0ms", "100ms", "1s", "10s", null]
                            slider.to: 1000
                            suffix: "ms"

                            onValueChanged: {
                                root.hasChanges = true;
                            }
                        }
                        Text {
                            id: boothDelayLabel

                            Layout.fillWidth: true
                            color: Theme.white
                            enabled: boothEnabled.value
                            font.pixelSize: 14
                            opacity: enabled ? 1 : 0.5
                            text: "Booth Output"

                            Mixxx.SettingParameter {
                                label: "Booth Output"
                            }
                        }
                        Skin.Slider {
                            id: boothDelaySlider

                            Layout.fillWidth: true
                            enabled: boothEnabled.value
                            markers: ["0ms", "100ms", "1s", "10s", null]
                            slider.to: 1000
                            suffix: "ms"
                            value: boothDelay.value

                            onValueChanged: {
                                root.hasChanges = true;
                            }
                        }
                        Text {
                            id: headphoneDelayLabel

                            Layout.fillWidth: true
                            color: Theme.white
                            enabled: headEnabled.value
                            font.pixelSize: 14
                            opacity: enabled ? 1 : 0.5
                            text: "Headphone Output"

                            Mixxx.SettingParameter {
                                label: "Headphone Output"
                            }
                        }
                        Skin.Slider {
                            id: headphoneDelaySlider

                            Layout.fillWidth: true
                            enabled: headEnabled.value
                            markers: ["0ms", "100ms", "1s", "10s", null]
                            slider.to: 1000
                            suffix: "ms"
                            value: headDelay.value

                            onValueChanged: {
                                root.hasChanges = true;
                            }
                        }
                    }
                }
                Mixxx.SettingGroup {
                    label: "Stats"
                    visible: root.selectedIndex == 2

                    onActivated: {
                        root.selectedIndex = 2;
                    }

                    Mixxx.SettingParameter {
                        label: "A white square"

                        Rectangle {
                            color: 'white'
                            height: 20
                            width: 20
                        }
                    }
                }
                Mixxx.SettingGroup {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    label: "Audio test"
                    visible: root.selectedIndex == 3

                    onActivated: {
                        root.selectedIndex = 3;
                    }

                    ColumnLayout {
                        id: audioTest

                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            color: Theme.white
                            font.pixelSize: 14
                            text: "Play a 440 Hz tone through an output. A button is off when that output is not configured."
                            wrapMode: Text.WordWrap

                            Mixxx.SettingParameter {
                                keywords: ["test", "tone", "sound", "check"]
                                label: "Audio test"
                            }
                        }
                        RowLayout {
                            spacing: 8

                            Skin.FormButton {
                                enabled: mainEnabled.value && !root.committing
                                opacity: enabled ? 1.0 : 0.5
                                primary: root.audioTestTarget === "main"
                                text: "Main"

                                onPressed: {
                                    root.startAudioTest("main");
                                }
                            }
                            Skin.FormButton {
                                enabled: boothEnabled.value && !root.committing
                                opacity: enabled ? 1.0 : 0.5
                                primary: root.audioTestTarget === "booth"
                                text: "Booth"

                                onPressed: {
                                    root.startAudioTest("booth");
                                }
                            }
                            Skin.FormButton {
                                enabled: headEnabled.value && !root.committing
                                opacity: enabled ? 1.0 : 0.5
                                primary: root.audioTestTarget === "headphones"
                                text: "Headphones"

                                onPressed: {
                                    root.startAudioTest("headphones");
                                }
                            }
                            Skin.FormButton {
                                backgroundColor: Theme.warningColor
                                enabled: root.audioTestTarget !== ""
                                opacity: enabled ? 1.0 : 0.5
                                text: "Stop"

                                onPressed: {
                                    root.stopAudioTest();
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        // Two live levels, because "I pressed it and heard
                        // nothing" has two very different causes and the ear
                        // cannot tell them apart: the tone never being made, or
                        // being made and never reaching the speaker.
                        RowLayout {
                            spacing: 12

                            Text {
                                color: root.audioTestTonePeak > 0 ? Theme.green : Theme.deckTextColor
                                font.pixelSize: 14
                                text: "Tone: " + (root.audioTestTonePeak < 0 ? "not tested" : Math.round(root.audioTestTonePeak * 100) + "%")
                            }
                            Text {
                                color: root.audioTestMainPeak > 0 ? Theme.green : Theme.deckTextColor
                                font.pixelSize: 14
                                text: "Main mix: " + (root.audioTestMainPeak < 0 ? "not tested" : Math.round(root.audioTestMainPeak * 100) + "%")
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            color: root.audioTestTarget === "" ? Theme.deckTextColor : Theme.blue
                            font.pixelSize: 14
                            text: {
                                switch (root.audioTestTarget) {
                                case "main":
                                    return "Sounding on the main output. Silence here with a device selected means the wrong device, a muted output or a dead cable -- the routing below says which device the main mix goes to.";
                                case "booth":
                                    // Worth saying rather than pretending the
                                    // booth is a separate signal: it is the main
                                    // mix again, so this tells apart a booth
                                    // wiring or gain problem from a main one.
                                    return "Sounding on the booth output, which carries the main mix through the booth gain. If Main was audible and this is not, the problem is the booth output, not the mix.";
                                case "headphones":
                                    return "Sounding on the headphone output only -- the tone is cued, not in the main mix, so main staying silent here is correct.";
                                default:
                                    return root.audioTestReading();
                                }
                            }
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
            Mixxx.SettingGroup {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: Math.max(router.mode == AudioRouter.Mode.Advanced ? 450 : 250, scrollView.height - tabSection.height - buttons.height - 15)
                Layout.minimumWidth: Math.max(600, scrollView.width)
                label: "Router"

                AudioRouter {
                    id: router

                    anchors.fill: parent
                }
                Rectangle {
                    anchors.fill: parent
                    color: Qt.alpha('grey', 0.3)
                    visible: root.committing

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        preventStealing: true

                        onWheel: mouse => {
                            mouse.accepted = true;
                        }
                    }
                }
            }
            RowLayout {
                id: buttons

                Layout.topMargin: 4

                Skin.FormButton {
                    backgroundColor: Theme.warningColor
                    enabled: !root.committing
                    opacity: enabled ? 1.0 : 0.5
                    text: "Cancel"
                    visible: root.hasChanges

                    onPressed: {
                        root.load();
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    id: errorMessage

                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: 16
                    color: Theme.warningColor
                    text: ""
                }
                Skin.FormButton {
                    primary: root.hasChanges
                    enabled: root.hasChanges && !root.committing
                    opacity: enabled ? 1.0 : 0.5
                    text: "Save"

                    onPressed: {
                        errorMessage.text = "";
                        root.save();
                    }
                }
            }
        }
    }
    Connections {
        function onCommitted(error) {
            root.committing = false;
            if (error) {
                errorMessage.text = error;
            }
            root.load();
        }

        target: Mixxx.SoundManager
    }
}
