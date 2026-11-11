# Quest performance diagnostics

`android_profile.c` is a standalone ARM64 launcher for this project's exact Android `libmain.so`, `libSDL2.so` and `libc++_shared.so`. It exposes `SDL_main` outside an Activity by overriding only SDL's Activity-only external-storage working directory and Android logging, then calling `SDL_SetMainReady`. Run with `SDL_VIDEODRIVER=dummy`, `-video none -sound none`, no input providers, and a bounded `-seconds_to_run`. No XR rendering occurs. This is a native CPU benchmark, not full VR FPS or a physical play test.

The compiler invocation uses the NDK's `aarch64-linux-android29-clang.cmd`, `-O2 -ldl -Wl,--export-dynamic`. Keep the executable/libraries and **copied** owned media and calibration fixtures in a dedicated `/data/local/tmp/vr4all-silent-scope-profile/` directory. Supply all native writable directories explicitly. Do not point benchmarks at the installed app's state or run them alongside a VR game. No root or broad app storage permission is needed. User-owned media is never bundled with this source.

`SILENT_SCOPE_CAPTURE=1` exercises production scene capture without loading XR. `SILENT_SCOPE_CPU_PROFILE=1` records coarse per-device costs to `SILENT_SCOPE_XR_LOG.engine` between native15–120 seconds. Native boot/input timing comes from `tools/boot-probe.lua` with its output directory explicitly supplied. `SILENT_SCOPE_FAST_SRAM=0` provides the old-dispatcher control in the same candidate binary; `SILENT_SCOPE_VERIFY_SRAM=1` compares every exercised fast read against the native memory dispatcher. Both-DSP and main-DSP recompilation were unstable and must not be used as a benchmark success result.

`scene_live_probe TRACE PREFIX FRAME_COUNT` benchmarks a fixed real captured scene offscreen in GLES at1536×1536 for both eyes. It reports CPU submission and completed stereo times separately. Omit the frame count for center/left/right PPM captures. RGBA readback supports Quest GLES. Real headset scheduling, compositor and native emulation are absent from this benchmark.

`dsp_pc_profile.h` is unused diagnostic instrumentation for counting interpreted PC addresses. It is not included in production. The temporary SHARC patch used for the investigation is retained under ignored `private/quest-profile/dsp-pc-profile.patch` and was reversed before the performance candidate was built. No instruction counting overhead remains in production.

Full commands, results and limitations are recorded in `overseer/changes/2026-11-01-silent-scope-performance/` in the overseer workspace.

`flycast_libretro_profile.cpp` is a separate offscreen EGL/GLES3 frontend for the isolated Dreamcast prototype's `flycast_libretro.so`. It does not load the arcade engine or switch the installed APK. Compile it with the existing NDK `aarch64-linux-android29-clang++.cmd`, C++17, `-O2 -Wall -Wextra -Wformat=2 -Werror -static-libstdc++ -lEGL -lGLESv3 -ldl -llog`, and the pinned Flycast `core/deps/libretro-common/include` directory. The neighboring policy header is part of its source.

```text
flycast_libretro_profile CORE_LIBRARY OWNED_CUE PRIVATE_ROOT [warmupNativeSeconds=35] [measureNativeSeconds=10] [maxWallSeconds=90] [maxRuns=100000] [--snapshots]
```

Use a fresh result directory under `/data/local/tmp/` or the task's private Android app `files/` profile area. Parent directories must already exist. `profile.json` is created exclusively; reusing a previous result directory fails. No media or production calibration is bundled. Run only the serialized owning-task probe, with an external process timeout as well: the internal wall limit cannot interrupt a core call that never returns. No device execution is performed by compilation.

Warmup/measurement gates and scripted input use delivered audio frames divided by reported sample rate, starting at the first input poll. That is an explicitly reported native-time **proxy**; buffer drops are not observable through libretro. The script pulses Dreamcast A (Retro B) during native0–15 and21–28, issues one Start pulse at20, then stays neutral after30. It retains stock200MHz, DSP, full audio synthesis and required RTT copy. Audio output is consumed without sleeping. No audio or an invalid/changing sample rate blocks the probe.

The report separates `retro_run` calls, fresh main hardware-video callbacks and null duplicate callbacks. `glFinish` after each run includes GPU completion and deliberately serializes CPU/GPU work; this is not XR refresh or full VR performance. Optional `warmup.rgba`/`end.rgba` are two bounded, tightly packed RGBA8 images with bottom-left origin; each file's dimensions/time are in `snapshotFiles`. Captures are outside the measured interval. Review them for actual 3D gameplay and native scope before interpreting any measured rate; the tool cannot classify a warning/title/menu automatically.

The focused standalone policy check is `tests/flycast_profile_policy_test.cpp` (C++17, no Android or libretro dependency). It checks numeric/path bounds, native audio gates, option invariants, finite input and separate video counts. Runtime EGL/core/callback behavior still requires the owning task's private Android validation.
