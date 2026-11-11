"""Narrow XR integration for source-attested native menus and weighted aim logs."""
from pathlib import Path
import difflib
import hashlib
import json

project=Path(__file__).resolve().parents[1]
engine=Path('D:/VR4ALL-SilentScope-Build/flycast-source')
path=engine/'core/rend/vr/xr_host.cpp'
before=path.read_text(encoding='utf-8')
text=before
def change(old,new):
    global text
    if text.count(old)!=1:raise SystemExit('Unexpected production anchor: '+old[:100])
    text=text.replace(old,new,1)

change('#include "dreamcast/dc_ui_domain.h"', '#include "dreamcast/dc_ui_domain.h"\n#include "dreamcast/dc_native_menus.h"\n#include "dreamcast/dc_menu_controls.h"')
change('ss::dc_mouse::MenuStickDirection ssNativeMenuStick;', 'ss::dc_mouse::MenuStickDirection ssNativeMenuStick;\nss::dc_menu_controls::Controller ssNativeControls;')
change('    ssSupport.reset();ssSmoothing.reset();ssNativeMenuStick.reset();','    ssSupport.reset();ssSmoothing.reset();ssNativeMenuStick.reset();ssNativeControls.reset();')
change('    const auto inputDomain=ss::dc_ui::classify(settings.content.gameId,ctx,ownedWorld);', '''    const auto menuSource=ss::dc_mouse::native_menu_snapshot();
    ss::dc_native_menus::NativeMenusProof nativeMenusProof;
    const bool menuGeometry=ctx&&nativeMenusProof.configure(settings.content.gameId,*ctx);
    const bool knownNativeMenu=menuSource.verified&&menuSource.context_active&&
        menuSource.kind!=ss::dc_menu::MenuKind::None&&menuSource.kind!=ss::dc_menu::MenuKind::Main;
    auto inputDomain=ss::dc_ui::classify(settings.content.gameId,ctx,ownedWorld);
    // Native source authority can block gameplay during an active menu even
    // when a changing/unknown raster has no actionable native item geometry.
    // This changes input only; native rendering/clipping ownership is intact.
    if(knownNativeMenu||menuGeometry)inputDomain=ss::dc_ui::Domain::NativeUi;''')
change('    if(ssPoseHand!=target.hand){resetSilentScopePose();ssPoseHand=target.hand;}', '''    if(ssPoseHand!=target.hand){resetSilentScopePose();ssPoseHand=target.hand;}
    const bool ownedControlFrame=ctx&&!ctx->isRTT&&ctx->framebufferWidth==640&&ctx->framebufferHeight==480&&
        ctx->dcCamera.supported&&ctx->dcCamera.generation&&ctx->dcCamera.submit_sequence;
    ssNativeControls.begin(knownNativeMenu?menuSource:ss::dc_menu::MenuFacts{},ownedControlFrame,
        menuGeometry?nativeMenusProof.page():-1);
    if(knownNativeMenu)target.ui_action=ssNativeControls.context();
    else if(menuGeometry)target.ui_action.kind=nativeMenusProof.kind();''')
change('    if(smoothed.valid)ssRifleModel=smoothed.model;', '''    if(smoothed.valid)ssRifleModel=smoothed.model;
    // Bounded real-runtime evidence distinguishes steady lag from repeated
    // lifecycle seeds. No per-frame log or gameplay target is inferred.
    static double aimReportAt=-1;static unsigned aimSamples{},aimRestarts{},aimInvalid{},aimReports{};
    if(native3d){++aimSamples;if(smoothed.restarted)++aimRestarts;if(!smoothed.valid)++aimInvalid;
        if(aimReports<48&&(aimReportAt<0||target.host_seconds-aimReportAt>=5)){
            double lag=-1;
            if(smoothed.valid&&supported.valid){const auto a=glm::normalize(glm::quat_cast(glm::mat3(supported.model)));
                const auto b=glm::normalize(glm::quat_cast(glm::mat3(smoothed.model)));
                lag=glm::degrees(2*std::acos(std::clamp(double(std::abs(glm::dot(a,b))),0.,1.)));}
            NOTICE_LOG(INPUT,"SS_AIM level=%d samples=%u restarts=%u invalid=%u supported=%u lag_deg=%.3f",
                ss::dc_settings::aim_smoothing_level(),aimSamples,aimRestarts,aimInvalid,unsigned(supported.supported),lag);
            aimReportAt=target.host_seconds;aimSamples=aimRestarts=aimInvalid=0;++aimReports;
        }
    }''')
change('    target.start=boolAction(startAction,Left)||boolAction(startAction,Right);', '''    target.start=boolAction(startAction,Left)||boolAction(startAction,Right);
    // Preserve the physical level even when the beam misses or its projection
    // is unavailable; captured valid hits are independently edge-latched.
    target.ui_select=nativeUi&&pull;''')
change('            glm::vec2 screen;float dist=-1;glm::vec3 native(0);','            glm::vec2 screen(-10000.f);float dist=-1;glm::vec3 native(0);')
change('''                if(target.ui_main_menu&&target.point_valid)
                    target.ui_pointer_item=mainMenuProof.hit(*ctx,screen.x*640.f,screen.y*480.f);''', '''                if(target.ui_main_menu&&target.point_valid)
                    target.ui_pointer_item=mainMenuProof.hit(*ctx,screen.x*640.f,screen.y*480.f);
                else if(knownNativeMenu&&target.point_valid){
                    const auto action=menuGeometry?nativeMenusProof.hit(*ctx,screen.x*640.f,screen.y*480.f):ss::dc_menu::MenuAction{};
                    target.ui_action=ssNativeControls.resolve(action,menuGeometry?nativeMenusProof.page():-1,
                        menuGeometry?unsigned(nativeMenusProof.source_context()):0,screen.x*640.f,screen.y*480.f);
                    if(pull&&target.ui_action.kind==ss::dc_menu::MenuKind::ModeSelect&&target.ui_action.verb==ss::dc_menu::MenuVerb::Focus)
                        target.ui_action.verb=ss::dc_menu::MenuVerb::Confirm;
                }''')

anchor='void drawSilentScopeInfo(const glm::mat4& vp){'
change(anchor, '''void drawSilentScopeNativeControls(const glm::mat4& vp){
    if(menuOpen||!focused)return;
    const float depth=ss::dc_settings::panel_distance_metres();const float focal=config::VrFocal;
    if(!std::isfinite(depth)||depth<=0||!std::isfinite(focal)||focal<=0)return;
    const float pixel=depth/focal;
    for(const auto& button:ssNativeControls.buttons()){
        const auto& r=button.bounds;const float hx=(r.x1-r.x0)*.5f*pixel,hy=(r.y1-r.y0)*.5f*pixel;
        const auto model=glm::translate(glm::mat4(1),glm::vec3(((r.x0+r.x1)*.5f-320)*pixel,
            -( (r.y0+r.y1)*.5f-240)*pixel,-depth));
        drawTexture(vp,model,0,hx,hy,glm::vec4(theme::Selection,.97f));
        int w=0,h=0;auto texture=textTexture(button.label,32,button.enabled?theme::Text:theme::Dim,w,h);
        if(texture&&w>0&&h>0){const float textHy=std::min(hy*.66f,hx*.9f*h/w);
            drawTexture(vp,model,texture,textHy*w/h,textHy);}
    }
}
''' + anchor)
change('''        if (menuOpen && !darkRoom)
''', '''        if(silentScope&&focused)drawSilentScopeNativeControls(eyeProjection(views[i].fov)*view);
''' + '''        if (menuOpen && !darkRoom)
''')

backup=project/'private/flycast-research/all-menu-ui-before'
backup.mkdir(exist_ok=True)
if (backup/'xr_host.cpp').exists():raise SystemExit('Preserve existing baseline; patcher is intentionally one-shot')
(backup/'xr_host.cpp').write_text(before,encoding='utf-8')
patch=''.join(difflib.unified_diff(before.splitlines(True),text.splitlines(True),fromfile='a/core/rend/vr/xr_host.cpp',tofile='b/core/rend/vr/xr_host.cpp'))
patchpath=project/'patches/dreamcast/0029-xr-native-submenu-controls.patch'
patchpath.write_text(patch,encoding='utf-8',newline='\n')
path.write_bytes(text.encode())
(backup/'receipt.json').write_text(json.dumps({'beforeSha256':hashlib.sha256(before.encode()).hexdigest(),'afterSha256':hashlib.sha256(text.encode()).hexdigest(),'patch':str(patchpath)},indent=2)+'\n')
print(patchpath)
