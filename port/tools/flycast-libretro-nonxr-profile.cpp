// SPDX-License-Identifier: GPL-2.0-or-later
// Explicitly opt-in libretro non-XR benchmark adapter for the 8398fead fork.
// Renderer/hands/lightgun code stays real. Native option defaults are retained,
// except VrXr=false. Empty keys cannot masquerade as exposed libretro settings.
#if !defined(LIBRETRO) || !defined(SILENT_SCOPE_LIBRETRO_NONXR_PROFILE) || defined(USE_OPENXR)
#error This diagnostic adapter requires LIBRETRO, the explicit profile flag, and no USE_OPENXR
#endif
#include "cfg/option.h"
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace {
// cfg.cpp's autoSaving bool is constant-initialized before dynamic initializers.
// Disabling it accesses no IniFile state and performs no I/O. libretro never
// calls config::open(); the real database therefore starts empty and stays only
// in memory, including native GunAngle migration writes during option loading.
struct DisableNativeConfigPersistence {
    DisableNativeConfigPersistence() { config::setAutoSave(false); }
} disableNativeConfigPersistence;
}

namespace config {
Option<bool> VrReproject("");
Option<float> VrFocal("", 640.f);
Option<int> VrProjAddr("", 0);
Option<int> VrViewportAddr("", 0);
Option<float> VrFovScale("", 1.f);
Option<float> VrHudW("", 1.f);
Option<bool> VrHideLetterbox("", true);
Option<float> VrHudDepth("", 40.f);
Option<float> VrYaw("", 0.f);
Option<float> VrPitch("", 0.f);
Option<float> VrWindowTanX("", 0.f);
Option<float> VrWindowTanY("", 0.f);
Option<float> VrOffsetX("", 0.f);
Option<float> VrOffsetY("", 0.f);
Option<float> VrOffsetZ("", 0.f);
Option<bool> VrAnimate("");
Option<int> VrUdpPort("", 0);
Option<bool> VrXr("", false);
#ifndef __ANDROID__
Option<bool> VrXrSim("", false);
Option<std::string> VrXrSimProfile("", "oculus/touch_controller");
Option<std::string> VrXrSimRenderModel("", "");
Option<bool> VrBuiltInGun("", false);
Option<std::string> VrWatchRam("", "");
Option<std::string> VrWatchTex("", "");
#endif
Option<bool> VrXrGun("", true);
Option<bool> VrWiden("", true);
Option<float> VrWorldScale("", 0.11f);
Option<float> VrXrResolution("", 1.f);
Option<int> VrMsaa("", 1);
Option<int> VrXrRefreshRate("", 72);
Option<bool> VrLaser("", true);
HudSizeOption VrHudSize;
ReticleModeOption VrReticleMode;
ReticleMinSizeOption VrReticleMinSize;
ReticleMaxSizeOption VrReticleMaxSize;
ReticleDistanceOption VrReticleShrinkDistance;
Option<bool> VrShowGun("", true);
Option<bool> VrShowControllers("", true);
Option<float> VrGunScale("", 0.68f);
Option<bool> VrGameHands("", true);
Option<float> VrHandScale("", 1.f);
Option<bool> VrSlideReload("", true);
Option<bool> VrStaffHand("", false);
Option<int> VrDebugShots("", 0);
Option<bool> VrDualWield("", true);
Option<bool> VrHideP2Prompt("", true);
Option<bool> VrHandsPrep("", false);
Option<int> VrHandsCaptureLimit("", 0);
Option<float> VrComfortStart("", 0.8f);
Option<float> VrComfortMin("", 0.4f);
Option<bool> VrAimWidened("", false);
Option<bool> VrWidenFov("", true);
Option<bool> VrDropShotMarker("", true);
Option<bool> VrDropShotFlash("", true);
Option<std::string> VrHaptics("", "medium");
Option<int> VrGun("", 1);
Option<std::string> VrGunFile("", "");
GunAngleOption VrGunAngle;
Option<int> VrReload("", 0);
Option<std::string> VrTwoPlayerP1Hand("", "left");
} // namespace config

// libretro owns its render context; the standalone UI queue does not exist.
// DebugShots=0 prevents this request in the benchmark. Never run GL work on the
// calling VBlank/emulator thread, silently discard it, or fake a successful shot.
void gui_runOnUiThread(std::function<void()> function)
{
    (void)function;
    std::fputs("Silent Scope non-XR benchmark: unexpected UI-thread request; aborting diagnostic.\n", stderr);
    std::fflush(stderr);
    std::abort();
}
