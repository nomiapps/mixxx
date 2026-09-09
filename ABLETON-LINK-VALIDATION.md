AI-generated implementation notes begin.

# Ableton Link test build

Worktree: `C:\StreamDeck\src\mixxx-link-sync`, branch `session/link-sync`.
Base: `eaf1d6d3906233cd915230baa3c336d522cc66fa`.
Upstream implementation: [Mixxx PR #10999](https://github.com/mixxxdj/mixxx/pull/10999),
reviewed at `3bba07ca7c` (fetched as `link-review/pr-10999`).

The port adds tempo and beat-phase synchronization, Link enable/disable and
peer-count controls, and external-sync latency compensation. Link controls are
available in the New UI toolbar, the Edge layout header, the upstream legacy
skins, and controller mappings through `[AbletonLink],sync_enabled` and
`[AbletonLink],num_peers`.

Local corrections include a stable clock regression using relative coordinates,
normalized negative beat positions, rejection of undefined tempo, removal of
per-buffer debug logging, and a preallocated latency-smoothing history. The
upstream timing tests were also registered with the build.

## Launch and setup

1. Close the currently running Mixxx instance.
2. Run `C:\StreamDeck\src\mixxx-link-sync\run-mine.ps1` in PowerShell.
   This launches this worktree's compiled executable with its separate
   `Mixxx-link-sync` profile. It requires the shared dependency bundle.
3. Enable Ableton Link on the MPC and connect it to the same local network.
4. Click **Link** in Mixxx. The number beside it is the connected peer count.
5. Enable **Sync** on participating Mixxx decks. Use the crown to select an
   explicit Mixxx deck leader when desired. Link peers can change session tempo.
6. Start the MPC pattern on the desired downbeat. This version synchronizes
   beats with a one-beat quantum; it does not identify bars or musical phrases.
   Transport start/stop synchronization is intentionally disabled, as upstream.

If audio is consistently early or late, use the external sync latency
compensation in the full **Preferences > Sound Hardware** dialog. The underlying
control is `[Master],externalSyncLatencyCompensation`, in milliseconds. Adjust
while comparing audible transients; a displayed phase match does not measure
the complete MPC and sound-device signal path.

## Hardware acceptance check

Use an accurately gridded track with clear transients and a simple MPC click or
drum pattern. Confirm peer discovery; tempo changes from both devices; audible
beat alignment over at least 10 minutes; smooth deck-leader handoffs; recovery
after cueing, scratching and stopping; Link disable/re-enable; and recovery
after a peer disconnects. Check 70/140 BPM material separately. Record the MPC
model/firmware, audio API, buffer size and any latency compensation.

Do not treat software peer tests as proof of audible MPC alignment. The actual
MPC/audio-driver acceptance check is still required before performance use.

## Verification results

- Application and full test target compiled successfully on Windows with MSVC,
  Qt 6, QML enabled, and the installed Ableton Link dependency.
- Before the final latency-history storage change, 127 affected tests passed:
  deck sync, timing, QML Link controls, mixer, player and controller regressions.
  Evidence: `link-sync-tests.xml`.
- The real two-peer test passed bidirectional tempo and beat-phase checks.
  Evidence: `link-peer-tests.xml` and `link-peer-tests.log`.
- After the final latency-history change, all 18 timing utility tests passed in
  a standalone target compiling the actual worktree sources and tests.
  Evidence: `link-timing-tests.xml` and `link-timing-tests.log`.
- The final application started the New UI offscreen with a disposable profile.
  The main QML components and waveform displays loaded; no Link component errors
  appeared. Existing offscreen/OpenGL and missing-control warnings remain.
- Windows Defender quarantined the rebuilt `build/mixxx-test.exe` as
  `Trojan:Win32/Bearfoos.B!ml`. The cause has not been independently established.
  No quarantined file was restored and no antivirus settings were changed.
  This blocked the final full-suite rerun. The application executable remained
  available and was used for the startup check.
- No audible MPC/physical audio-device test has been performed.
## Build and review

The build runner is `C:\Users\User\Documents\ChatGPT\Mixxx\build-link-sync.bat`.
It configures QML and builds `mixxx` and `mixxx-test` in this worktree, reusing the
installed dependency bundle. Test logs and XML results are alongside that runner.

The network integration test is opt-in:
`mixxx-test.exe --gtest_filter=EngineSyncTest.DISABLED_LinkPeerTempoAndPhase --gtest_also_run_disabled_tests`.
It starts a second real Link peer and tests discovery, tempo updates in both
directions, phase agreement and disabling Link. Run it away from live sessions
because it changes the shared Link tempo.

Changes remain local for review and hardware testing. The shared checkout and
its executable have not been updated by this work.

AI-generated implementation notes end.
