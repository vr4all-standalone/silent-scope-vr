"""Apply narrowly checked native-menu/pose integration to the current SS fork."""
from pathlib import Path
import difflib

project=Path(__file__).resolve().parents[1]
engine=Path('D:/VR4ALL-SilentScope-Build/flycast-source')
files={name:(engine/name).read_text(encoding='utf-8') for name in
       ['core/rend/vr/xr_host.cpp','core/rend/vr/xr_menu.cpp','core/rend/vr/xr_menu.h']}
original=files.copy()
def change(name,old,new):
    if files[name].count(old)!=1:raise RuntimeError('Unexpected current integration: '+old[:90])
    files[name]=files[name].replace(old,new)

h='core/rend/vr/xr_host.cpp'
change(h,'#include "dreamcast/rifle_support.h"', '#include "dreamcast/rifle_support.h"\n#include "dreamcast/rifle_smoothing.h"\n#include "dreamcast/dc_menu_stick.h"')
# Existing lifecycle reset sites all belong to the SS adapter. Define the helper
# only after replacing those sites, avoiding replacement inside its own body.
if not 3<=files[h].count('ssSupport.reset();')<=12:raise RuntimeError('Unexpected pose reset sites')
files[h]=files[h].replace('ssSupport.reset();','resetSilentScopePose();')
change(h,'ss::RifleSupport ssSupport;', '''ss::RifleSupport ssSupport;
ss::RifleSmoothing ssSmoothing;
std::array<ss::GripLatch,2> ssGripLatch;
ss::dc_mouse::MenuStickDirection ssNativeMenuStick;
int ssPoseHand=-1;
bool ssSupported{};''')
change(h,'XrTime ssSecondarySince{};', '''XrTime ssSecondarySince{};
void resetSilentScopePose(){
    ssSupport.reset();ssSmoothing.reset();ssNativeMenuStick.reset();
    for(auto& grip:ssGripLatch)grip.reset();
    ssPoseHand=-1;ssSupported=false;
}''')
begin=files[h].index('void updateSilentScope(XrTime time)')
end=files[h].index('// The rifle renderer uses raw GL calls;',begin)
body=files[h][begin:end]
def body_change(old,new):
    global body
    if body.count(old)!=1:raise RuntimeError('Unexpected current SS pose block: '+old[:90])
    body=body.replace(old,new)

domain='''    const auto* ctx=gles_vr_frame();
    if(ctx&&ctx->dcCamera.generation!=ssNativeGeneration){
        ssNativeGeneration=ctx->dcCamera.generation;ss::dc_xr::input_bridge().invalidate();resetSilentScopePose();ssSubmittedFireWas=false;
    }
    const bool ownedWorld=ctx && gameCamera(*ctx).valid && silentScopeWorldGeometry(*ctx)
        &&config::VrReproject&&!gles_vr_showing_framebuffer();
    const auto inputDomain=ss::dc_ui::classify(settings.content.gameId,ctx,ownedWorld);
    static auto previousInputDomain=ss::dc_ui::Domain::Unknown;
    if(inputDomain!=previousInputDomain){
        previousInputDomain=inputDomain;
        // Unknown must not masquerade as physical release then replay a held
        // trigger when World returns. Keep raw trigger hysteresis, rearm bridge.
        ss::dc_xr::input_bridge().invalidate();ssSubmittedFireWas=false;
    }
    const bool native3d=inputDomain==ss::dc_ui::Domain::World;
    const bool nativeUi=inputDomain==ss::dc_ui::Domain::NativeUi;
    target.native_ui=nativeUi;
'''
body_change(domain,'')
domain=domain.replace('ss::dc_xr::input_bridge().invalidate();ssSubmittedFireWas=false;',
                      'ss::dc_xr::input_bridge().invalidate();resetSilentScopePose();ssSubmittedFireWas=false;')
body_change('    XrSpaceLocation aim[2];bool tracked[2];locateHands(time,aim,tracked);',domain+'''    // Cancel old generation/owner state before solving this frame's pose.
    if(ssPoseHand!=target.hand){resetSilentScopePose();ssPoseHand=target.hand;}
    XrSpaceLocation aim[2];bool tracked[2];locateHands(time,aim,tracked);''')
body_change('        hands[h]={model,tracked[h],floatAction(gripAction,h)>.7f};', '''        XrActionStateGetInfo gripInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        gripInfo.action=gripAction;gripInfo.subactionPath=handPaths[h];
        XrActionStateFloat squeeze{XR_TYPE_ACTION_STATE_FLOAT};
        const bool gripKnown=XR_SUCCEEDED(XR(xrGetActionStateFloat)(session,&gripInfo,&squeeze))&&squeeze.isActive&&
            std::isfinite(squeeze.currentState)&&squeeze.currentState>=0&&squeeze.currentState<=1&&tracked[h]&&ssHeadTracked;
        bool held=true;
        if(gripKnown)held=ssGripLatch[h].update(squeeze.currentState);else ssGripLatch[h].reset();
        hands[h]={model,tracked[h],held,gripKnown&&ssGripLatch[h].valid()};''')
body_change('''    const auto supported=ssSupport.update(hands,{target.focused&&ssHeadTracked,false,target.hand});
    if(supported.valid)ssRifleModel=supported.model;
''', '''    XrActionStateGetInfo stickInfo{XR_TYPE_ACTION_STATE_GET_INFO};stickInfo.action=stickAction;
    XrActionStateVector2f nativeStick{XR_TYPE_ACTION_STATE_VECTOR2F};
    const bool stickQueried=XR_SUCCEEDED(XR(xrGetActionStateVector2f)(session,&stickInfo,&nativeStick));
    target.ui_navigation=ssNativeMenuStick.update_sample({stickQueried,nativeStick.isActive!=0,
        nativeStick.currentState.x,nativeStick.currentState.y},target.focused&&target.tracked&&nativeUi,target.hand);
    const auto supported=ssSupport.update(hands,{target.focused&&target.tracked&&native3d,false,target.hand});
    const auto smoothed=ssSmoothing.update(supported.model,{target.host_seconds,target.hand,
        ss::dc_settings::aim_smoothing_level(),supported.supported,target.focused&&target.tracked&&native3d&&supported.valid});
    if(smoothed.valid)ssRifleModel=smoothed.model;
    if(supported.supported&&!ssSupported)haptic(1-target.hand,.025f,.25f);
    ssSupported=supported.supported;
''')
body_change('ssRifleValid=config::VrXrGun&&native3d&&target.tracked&&supported.valid&&!menuHowToPlay;',
            'ssRifleValid=config::VrXrGun&&native3d&&target.tracked&&smoothed.valid&&!menuHowToPlay;')
body_change('if(target.tracked&&(nativeUi||supported.valid)&&inputDomain!=ss::dc_ui::Domain::Unknown)',
            'if(target.tracked&&(nativeUi||smoothed.valid)&&inputDomain!=ss::dc_ui::Domain::Unknown)')
files[h]=files[h][:begin]+body+files[h][end:]
change(h,'drawGunGlow(viewProj, eyePos, pointers[hand], first);',
            'drawGunGlow(viewProj, eyePos, pointers[hand], first, drawRifle&&!pointers[hand].menuPointer);')
change(h,'''const char* lines[]={"Trigger: Fire / select native screen", "A / X: Start / Pause", "B / Y: hold 0.8s for Menu", "Offhand grip near fore-end: Support", "Left Menu: Settings"};
        for(int n=0;n<5;++n)''', '''const char* lines[]={"Trigger: Fire / select native screen", "A / X: Start / Pause", "Either stick Up/Down: Native menus", "B / Y: hold 0.8s for Menu", "Hold offhand Grip near fore-end: Support", "Aim smoothing: VR settings", "Left Menu: Settings"};
        for(int n=0;n<7;++n)''')

m='core/rend/vr/xr_menu.cpp'
change(m,'"Quit", "Hand", "FPS counter" };','"Quit", "Hand", "FPS counter", "Aim smoothing" };')
change(m,'''static const std::vector<int> silentScope { Resume, Recenter, Gun, GunAngle, Hand, Reload, TwoPlayers, Laser, Reticle,
        ReticleMinSize, ReticleMaxSize, ReticleShrinkDistance, Haptics, WorldSize, PanelDistance, HudSize, WiderView,
        HowToPlay, ServiceMenu, FpsCounter, ResetDefaults, Quit };''', '''static const std::vector<int> silentScope { Resume, Recenter, Gun, GunAngle, AimSmoothing, Hand, Reload, TwoPlayers, Laser, Reticle,
        ReticleMinSize, ReticleMaxSize, ReticleShrinkDistance, Haptics, WorldSize, PanelDistance, HudSize, WiderView,
        HowToPlay, FpsCounter, ResetDefaults, Quit };''')
change(m,'case FpsCounter: setting=dc::Setting::FpsCounter; return true;',
         'case FpsCounter: setting=dc::Setting::FpsCounter; return true;\n    case AimSmoothing: setting=dc::Setting::AimSmoothing; return true;')
change('core/rend/vr/xr_menu.h','Quit, Hand, FpsCounter, Rows };','Quit, Hand, FpsCounter, AimSmoothing, Rows };')

snapshot=project/'private/flycast-research/sniper-handling-before';snapshot.mkdir(exist_ok=True)
patches={}
for name,text in files.items():
    (snapshot/Path(name).name).write_text(original[name],encoding='utf-8')
    patch=''.join(difflib.unified_diff(original[name].splitlines(True),text.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
    patch_name='0024-'+Path(name).stem+'-sniper-handling.patch'
    patches[patch_name]=patches.get(patch_name,'')+patch
    (engine/name).write_text(text,encoding='utf-8',newline='')
for name,patch in patches.items():
    (project/'patches/dreamcast'/name).write_text(patch,encoding='utf-8')
print('Integrated native stick, shared supported/smoothed pose, reticle depth flag and smoothing menu')
