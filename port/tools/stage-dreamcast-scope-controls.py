"""Stage the Silent Scope stability and magnification controls in Flycast."""
from pathlib import Path
import difflib
import shutil

project = Path(__file__).resolve().parents[1]
engine = Path("D:/VR4ALL-SilentScope-Build/flycast-source")
changes = {
    "xr_host.cpp": [
        ('#include "dreamcast/rifle_smoothing.h"', '#include "dreamcast/rifle_smoothing.h"\n#include "dreamcast/rifle_scope_stability.h"'),
        ("ss::RifleSmoothing ssSmoothing;", "ss::RifleSmoothing ssSmoothing;\nss::RifleScopeStability ssScopeStability;\nbool ssScopeZoomArmed{};"),
        ("ssSupport.reset();ssSmoothing.reset();", "ssSupport.reset();ssSmoothing.reset();ssScopeStability.reset();ssScopeZoomArmed=false;"),
        ('"Menus (D-pad), world size", XR_ACTION_TYPE_VECTOR2F_INPUT)', '"Menus (D-pad), world size / scope zoom", XR_ACTION_TYPE_VECTOR2F_INPUT, true)'),
        ("    if(smoothed.valid)ssRifleModel=smoothed.model;", """    if (smoothed.restarted)
        ssScopeStability.reset();
    if (smoothed.valid) {
        const auto local = glm::inverse(smoothed.model);
        bool aligned = false;
        for (const auto& view : views) {
            const auto eye = glm::vec3(local * glm::vec4(relativePosition(view.pose.position), 1)) - ss::dc_rifle::ocular;
            aligned |= ss::scope_visibility({eye.x, eye.y, eye.z}, {}).visible;
        }
        ssRifleModel = ssScopeStability.update(smoothed.model, target.host_seconds, aligned, ss::dc_settings::scope_stability());
    } else {
        ssScopeStability.reset();
    }
    XrActionStateGetInfo zoomInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    zoomInfo.action = stickAction;
    zoomInfo.subactionPath = handPaths[Right];
    XrActionStateVector2f zoomStick{XR_TYPE_ACTION_STATE_VECTOR2F};
    const bool zoomAvailable = native3d && target.tracked &&
        XR_SUCCEEDED(XR(xrGetActionStateVector2f)(session, &zoomInfo, &zoomStick)) && zoomStick.isActive &&
        std::isfinite(zoomStick.currentState.y) && std::abs(zoomStick.currentState.y) <= 1.f;
    if (!zoomAvailable) {
        ssScopeZoomArmed = false;
    } else if (std::abs(zoomStick.currentState.y) <= .3f) {
        ssScopeZoomArmed = true;
    } else if (ssScopeZoomArmed && std::abs(zoomStick.currentState.y) >= .6f) {
        ssScopeZoomArmed = false;
        if (ss::dc_settings::change(ss::dc_settings::Setting::ScopeZoom, zoomStick.currentState.y > 0 ? 1 : -1))
            haptic(Right, .025f, .2f);
    }"""),
        ("scope.texture,1.f,!scope.content_top_at_v0);", "scope.texture,1.f,!scope.content_top_at_v0,float(ss::dc_settings::scope_magnification()));"),
        ('"Aim smoothing: VR Menu (- / +)",', '"Aim smoothing / Scope stability: VR Menu", "Right stick Up / Down: scope zoom",'),
        ("for(int n=0;n<7;++n){int w=0,h=0;auto tex=textTexture(lines[n]", "for(size_t n=0;n<sizeof(lines)/sizeof(lines[0]);++n){int w=0,h=0;auto tex=textTexture(lines[n]"),
    ],
    "xr_menu.h": [("FpsCounter, AimSmoothing, Rows", "FpsCounter, AimSmoothing, ScopeStability, ScopeZoom, Rows")],
    "xr_menu.cpp": [
        ("constexpr int SsRowsPerPage = 11;", "constexpr int SsRowsPerPage = 12;"),
        ('"FPS counter", "Aim smoothing" };', '"FPS counter", "Aim smoothing", "Scope stability", "Scope zoom" };'),
        ("GunAngle, AimSmoothing, Hand", "GunAngle, AimSmoothing, ScopeStability, ScopeZoom, Hand"),
        ("    case AimSmoothing: setting=dc::Setting::AimSmoothing; return true;",
         "    case AimSmoothing: setting=dc::Setting::AimSmoothing; return true;\n    case ScopeStability: setting=dc::Setting::ScopeStability; return true;\n    case ScopeZoom: setting=dc::Setting::ScopeZoom; return true;"),
    ],
}
staged = []
for name, edits in changes.items():
    relative = f"core/rend/vr/{name}"
    path = engine / relative
    before = path.read_text(encoding="utf-8")
    after = before
    for old, new in edits:
        if after.count(old) != 1:
            raise SystemExit(f"Unexpected anchor in {name}: {old}")
        after = after.replace(old, new, 1)
    staged.append((relative, before, after))

files = [project / "native" / name for name in
         ("rifle_scope_stability.h", "scope_optics.h", "rifle_renderer.h", "rifle_renderer.cpp")]
files += [project / "native/dreamcast" / name for name in ("dc_settings.h", "dc_settings.cpp", "dc_rifle_fit.h")]
for source in files:
    shutil.copyfile(source, engine / "core/dreamcast" / source.name)

patch = []
for relative, before, after in staged:
    (engine / relative).write_text(after, encoding="utf-8", newline="\n")
    patch.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                      f"a/{relative}", f"b/{relative}"))
(project / "patches/dreamcast/0033-scope-stability-and-zoom.patch").write_text("".join(patch), newline="\n")
