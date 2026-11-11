"""Stage the guarded native scope focal change and cheek anchor."""
from pathlib import Path
import difflib
import shutil

project = Path(__file__).resolve().parents[1]
engine = Path("D:/VR4ALL-SilentScope-Build/flycast-source")
changes = {
    "core/rend/vr/xr_host.cpp": [
        ('#include "dreamcast/rifle_scope_stability.h"',
         '#include "dreamcast/rifle_scope_stability.h"\n#include "dreamcast/dc_scope_camera.h"'),
        ("ssScopeStability.update(smoothed.model, target.host_seconds, aligned, ss::dc_settings::scope_stability_level());",
         """ssScopeStability.update(smoothed.model, glm::vec3(headPose()[3]), ss::dc_rifle::ocular,
            {0, ss::dc_rifle::foregrip.height, ss::dc_rifle::foregrip.front},
            target.host_seconds, aligned, ss::dc_settings::scope_stability_level());"""),
        ("    // Bounded real-runtime evidence distinguishes steady lag",
         """    ss::dreamcast::scope_camera_magnification.store(ss::dc_settings::scope_magnification(), std::memory_order_relaxed);
    // Bounded real-runtime evidence distinguishes steady lag"""),
        ("scope.texture,1.f,!scope.content_top_at_v0,float(ss::dc_settings::scope_magnification())",
         "scope.texture,1.f,!scope.content_top_at_v0"),
        ('"Aim smoothing / Scope stability: VR Menu"', '"Scope stability: raise rifle to eyepiece"'),
    ],
    "core/rend/vr_reproject.cpp": [
        ('#include "dreamcast/dc_camera_focal_probe.h"', '#include "dreamcast/dc_camera_focal_probe.h"\n#include "dreamcast/dc_scope_camera.h"'),
        ("    const auto decision=production?persistent.store:cameraFocalProbe().evaluate(now,loaded,caller,mode,ctx->fpscr.full,guard,ctx->sr.FD);",
         """    auto decision=production?persistent.store:cameraFocalProbe().evaluate(now,loaded,caller,mode,ctx->fpscr.full,guard,ctx->sr.FD);
    float scopeScale = production ? scope_camera_magnification.load(std::memory_order_relaxed) : 1.f;
    bool scopeRequested = production;
#if defined(_WIN32) && !defined(USE_OPENXR) && !defined(LIBRETRO)
    // Controlled comparison on the existing owned native checkpoint.
    if (cameraFocalProbe().enabled() && std::getenv("SILENT_SCOPE_DC_NATIVE_SCOPE_PROBE")) {
        scopeRequested = true;
        scopeScale = now < 46 ? .2f : now < 50 ? 1.f : now < 54 ? 2.f : 5.f;
    }
#endif
    const auto scope = decide_scope_camera(scopeScale,loaded,caller,mode,ctx->fpscr.full,
        scopeRequested && codeGuard && ReadMem32_nommu(ScopeSourceAddress)==loaded,ctx->sr.FD);
    if (scope.eligible)
        decision = scope;"""),
        ("        if(production){\n            const bool stored=ReadMem32_nommu(0x8cd1f9c0)==decision.effective_bits;",
         """        if (scope.eligible) {
            static float lastScopeScale = 0.f;
            if (scopeScale != lastScopeScale) {
                lastScopeScale = scopeScale;
                const auto actual = ReadMem32_nommu(0x8cd1f9c0);
                NOTICE_LOG(RENDERER,"SS_SCOPE_CAMERA native_s=%.3f scale=%.2f stock=%08x effective=%08x stored=%08x",
                    now,scopeScale,loaded,scope.effective_bits,actual);
                std::fprintf(stderr,"SS_SCOPE_CAMERA native_s=%.3f scale=%.2f stock=%08x effective=%08x stored=%08x\\n",
                    now,scopeScale,loaded,scope.effective_bits,actual);
            }
        }
        if(production && !scope.eligible){
            const bool stored=ReadMem32_nommu(0x8cd1f9c0)==decision.effective_bits;"""),
    ],
}

staged = []
for relative, edits in changes.items():
    before = (engine / relative).read_text(encoding="utf-8")
    after = before
    for old, new in edits:
        if after.count(old) != 1:
            raise SystemExit(f"Unexpected anchor in {relative}: {old}")
        after = after.replace(old, new, 1)
    staged.append((relative, before, after))
for source in (project / "native/rifle_scope_stability.h", project / "native/dreamcast/dc_scope_camera.h",
               project / "native/scope_optics.h", project / "native/rifle_renderer.h", project / "native/rifle_renderer.cpp"):
    shutil.copyfile(source, engine / "core/dreamcast" / source.name)
patch = []
for relative, before, after in staged:
    (engine / relative).write_text(after, encoding="utf-8", newline="\n")
    patch.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                      f"a/{relative}", f"b/{relative}"))
(project / "patches/dreamcast/0035-native-scope-camera-and-cheek-anchor.patch").write_text("".join(patch), newline="\n")
