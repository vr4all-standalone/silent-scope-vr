# Silent Scope VR development

The current public release being prepared is **0.0.68**. Current installation,
controls and release information are in [the project page](docs/github/README.md).
The development notes below preserve earlier milestones and their controls.

Silent Scope 1 uses the Dreamcast version for the current Quest candidate. The tracked sniper rifle displays the game's actual independent scope render inside its physical eyepiece. Arcade and manual bolt/magazine Simulation are implemented. Silent Scope 2 remains later work.

## Current candidate: 0.0.53

The signed Quest candidate puts two labeled, down-facing rifles in front of the player: Arcade left and Simulation right. Their grips sit0.45m forward,0.40m below the head and0.18m to either side. Choose by grabbing; the Arcade/Simulation selector is removed from VR settings. Grip the rear handle with the configured firing hand, or the fore-end with the support hand, to pick one up. While either hand holds a rifle, the other rifle and both rack labels are hidden. Release both grips to show both rifles again before grabbing the other. Two-hand support and rear hand transfers work with either rifle. Release Trigger after pickup or rear regrab before firing. Switching retains the Simulation chamber/manual-reload state and reconciles its ammunition against the native HUD.

This includes 0.0.50's shared card settings, controller help, Laser/Crosshair shortcut, Off/On/Fade and separate rifle sizes. Firing-hand B/Y starts or pauses; A/X holds Focus. The firing-hand stick changes scope zoom and its click toggles Laser/Crosshair; the spare stick click recenters. Hold spare-hand B/Y for settings, or use the runtime's Menu action. Scope fits remain Arcade27mm/125% and Simulation8mm/110% unless separately saved choices exist.

The APK now bundles the same controller GLBs and button anchors as Confidential Mission, with model attribution. How to play retains Silent Scope's mirrored Start/Focus and rifle-specific controls while using the shared label layout. Both bundled and fallback anchor layouts pass6000 real-font checks. Scope0.2x/0.5x also widen the game's separate native actor visibility planes to match the applied SDK focal; projection magnification remains unchanged. Native trace/math checks pass, but the reported enemy disappearance still needs reproduction in the affected headset scene. The user explicitly deferred the Story/Time Trial game menu; it is unchanged here.

77/77 host/GPU checks pass. Package identity, preserved signing certificate, packaged controller files and engine hash are verified. Quest0.0.53/code55 is installed and hash-verified on Quest3 with data preserved and NativeGLActivity resumed. Quest3S retains0.0.52. Physical acceptance of pickup-only selection and visibility remains pending. See the current build report at `../../overseer/changes/2026-11-04-silent-scope-pickup-only-menu/report.md`.

## Historical 0.0.8 notes

Quest **0.0.8/code10 is installed and its APK hash is verified**. Package `com.vrlightgunhub.silentscope`, label `Silent Scope - VR4ALL`, signing identity and existing app data are preserved. Accepted0.0.6 and0.0.7 candidates remain available as rollbacks. No GitHub publication occurred.

The console candidate includes immersive source-owned geometry, native Wider view, the physical rifle/native scope, optional offhand support and the purple shared VR settings menu. Unknown geometry fails safely to the native view. Full native scope VRAM copying remains enabled; the eyepiece uses its own owned GPU copy, never a crop of the main image.

Software verification for0.0.8 passes66/66 host/real-GL checks. The original-game production bridge/Consumer/Maple path selects0→1→2→3 and opens Ranking from a captured pointer click. NativeGLActivity is running on the user-awakened Quest; physical pointer acceptance remains pending. The user confirms immersive3D, native-menu orientation, stable scope and moving-aim trigger registration in0.0.6, and held fore-end support plus custom-reticle occlusion in0.0.7. Smoothing exposes saved Off/Low/Medium/High controls; physical tuning remains unconfirmed. Four-image scope history and accepted trigger behavior are preserved.

The earlier production camera/input run passes1,194/1,194 settled frames, actual Wide/Original transitions, expanded native aiming limits, signed aiming goals and the native Hurry up advisory. The initial checkpoint buffer remains safely unowned. Earlier native firing has three ammunition-consumption events; no hit or new short-tap guest acceptance is inferred.

Quest offscreen ARM64 testing completes16.015audio-derived native seconds in4.299wall seconds with480fresh callbacks/one duplicate. This is native throughput, **not headset FPS**. Live0.0.6 gameplay separately measures71.93–72.16Hz XR/29.56–30.64 native updates per second in the captured late active windows. Register tracing remains disabled. Version0.0.7 launched and was physically played; its brief early telemetry does not establish settled gameplay FPS. PCVR, later levels and far-angle native RTT camera correspondence remain unverified.

Installed APK: `artifacts/silent-scope-vr-quest-v0.0.8.apk`,18,338,757bytes, SHA256 `fa0193bbe5108cb08ef830b129f7f0d206578773722a852b9d30983dbfbf76ef`.

## Controls and settings

- Trigger fires during verified gameplay. In the native Main Menu, point at the desired label and pull Trigger; blank space does not confirm the old selection.
- A/X sends an explicit native Start/Pause. It does not insert arcade credits in the console version.
- Main Menu laser selection uses actual glyph bounds and the game's acknowledged wheel path. A click captures its item, waits for native highlight and wheel neutral, then sends a bounded confirmation press. Native Options values and pointer interaction with other submenus are not added.
- Menu opens VR settings where the runtime supplies that action. Hold either B/Y for0.8seconds as the reliable fallback; a later short B/Y returns/closes.
- Either tracked controller can point/select in settings. Thumbsticks navigate/adjust, A/X activates, and Previous/Next page through11rows.
- Left thumbstick click recenters; right thumbstick click toggles Laser. Hand chooses Left/Right without changing native player identity.
- Current builds put Arcade in front on the left and Simulation in front on the right, pointing down with labels above. Grip the handle with your firing hand, or the fore-end with your support hand, to pick one up. Release both grips to return it, then grab the other rifle. Only one rifle can be equipped. After interruption, release Grip before grabbing again.
- Hold offhand Grip near the fore-end to support either rifle. Releasing the rear hand leaves it in the support hand; a rear regrab preserves aim. Hand positions determine forward aim; wrist twist supplies roll. Simulation retains its chamber and manual-reload state when holstered, with native HUD ammo reconciliation on return.
- Aim smoothing, immediately below Gun angle in VR settings, offers Off/Low/Medium/High using −/+, defaultLow, with a saved preference and Reset. It steadies fine rotation while following fast turns more quickly. Rifle, optic, muzzle and native aim use one shared supported/filtered pose; the primary grip stays attached.
- The custom world reticle is occluded by the physical rifle and optic; the game's original crosshair stays in its native scope image. Laser remains independent.
- Menu, focus, tracking, hand and recenter transitions release native input. Quit requires a fresh confirmation.

Settings include hand, gun angle, Aim smoothing, Laser, Custom/None reticle with size/distance, Haptics, World size, Panel distance, actual-Wider HUD size, Wider view, help, separate XR/native-main counters and Reset. Gun is fixed Rifle, Reload is Automatic, Two players is unavailable, and the unsupported arcade Service row is omitted in this console version. Main Menu pointer selection preserves native selection/confirmation logic.

Defaults: Right hand, Laser On, Custom25%minimum/100%maximum/6m shrink, Medium haptics, World size0.1, Panel distance1.8m, HUD100%, Wider On, FPS counter Off. Displayed Gun angle0 preserves the physical minus10degree baseline. Settings save atomically. Reset preserves owned media and native state. Target-attached markers keep100% bearing-preserving sizing; known full-screen HUD scales only in actual Wider view.

Known unsupported native geometry/messages retain safe fallback. No complete-game, physical-play or hit-accuracy claim follows from software checks. Haptics acknowledge a fresh firing gesture rather than a native hit.

## Owned media

Import the user's own US Dreamcast `Silent Scope (USA).cue` and its three matching BIN tracks. The launcher verifies the exact owned-file manifest and supports Android's document picker or its own external `files/games/dreamcast` directory. No BIOS, game image, native save/timekeeper or signing key is distributed. Console preferences/native saves use a separate private `dreamcast/` home; earlier arcade files remain preserved.

## Build and evidence

The isolated engine is `D:/VR4ALL-SilentScope-Build/flycast-source`, base `8398fead532ede9d52c37addef3353611b6c5ce7`. It uses29 pinned local dependencies rather than another game's dirty working files. Adapter sources are in `native/dreamcast/`; integration patches and exact source manifests accompany the change report.

Run `tools/build-host.ps1` for the production/GPU suite. With the audited engine integration in place, `tools/build-flycast-quest-apk.ps1 -Offline` builds the native Quest engine and signed APK. Production omits DEBUGFAST/register tracing; `build-flycast-quest-engine.ps1 -VerboseLogging` is diagnostic only. `tools/build-flycast-native.ps1` builds the non-XR Windows diagnostic. `tools/build-flycast-android-profile.ps1` and `tools/run-quest-dreamcast-camera-probe.ps1` are bounded offscreen diagnostics, not install or physical-play checks.

`tools/run-dreamcast-controlled-probe.ps1` operates only a private executable/media checkpoint. Synthetic native input, forced bounded termination, offscreen rendering, real XR launch and physical play are distinct results. Private checkpoints and owned-media evidence are excluded from distributable artifacts.

The earlier MAME/Hornet arcade research remains in `engine-lock.json`, `patches/0001-silent-scope-bridge.patch`, the original `native/` host, `tools/build-engine.ps1` and private receipts. Its main-SHARC bottleneck caused roughly11–22native scene updates/sec in the tested headset scene. Experimental main-CPU recompiler changes were reversed. Those arcade tools do not build the installed console candidate.

The current report is `overseer/changes/2026-11-02-silent-scope-sniper-handling/report.md`; accepted0.0.6 evidence is in `overseer/changes/2026-11-02-silent-scope-menu-scope/report.md`, and original history is in `overseer/changes/2026-11-01-silent-scope-complete-vr/report.md`. Release/publication requires separate approval.
