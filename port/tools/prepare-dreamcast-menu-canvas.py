"""Incremental source/proof-qualified native menu presentation and canvas clip."""
from pathlib import Path
import difflib, hashlib, json
project=Path(__file__).resolve().parents[1]
engine=Path('D:/VR4ALL-SilentScope-Build/flycast-source')
backup=project/'private/flycast-research/menu-canvas-before'
backup.mkdir(parents=True,exist_ok=True)
entries=[]
def edit(relative, changes):
    path=engine/relative
    before=path.read_text(encoding='utf-8')
    after=before
    for old,new in changes:
        if after.count(old)!=1: raise SystemExit('Unexpected anchor '+relative+': '+old[:100])
        after=after.replace(old,new,1)
    saved=backup/path.name
    if saved.exists(): raise SystemExit('Backup already exists: '+str(saved))
    saved.write_text(before,encoding='utf-8',newline='\n')
    path.write_text(after,encoding='utf-8',newline='\n')
    entries.append({'path':relative,'before_sha256':hashlib.sha256(before.encode()).hexdigest(),
        'after_sha256':hashlib.sha256(after.encode()).hexdigest()})
    return ''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),
        fromfile='a/'+relative,tofile='b/'+relative))
patch=edit('core/rend/gles/gles.cpp',[
('out highp vec3 vtx_uv;','out highp vec3 vtx_uv;\n#if VR_REPROJECT == 1\nout highp vec3 vtx_native_canvas;\n#endif'),
('\thighp vec4 vpos = ndcMat * in_pos;','\thighp vec4 vpos = ndcMat * in_pos;\n#if VR_REPROJECT == 1\n    vtx_native_canvas = vec3(0.0);\n#endif'),
('            highp vec2 ndc = vpos.xy / vpos.w;','            highp vec2 ndc = vpos.xy / vpos.w;\n            vtx_native_canvas = vec3(ndc * 0.5 + 0.5, 1.0);'),
('in highp vec3 vtx_uv;\n\nlowp float fog_mode2','in highp vec3 vtx_uv;\n#if VR_REPROJECT == 1\nin highp vec3 vtx_native_canvas;\n#endif\n\nlowp float fog_mode2'),
('\t// Clip inside the box','''    // SS_NATIVE_CANVAS_CLIP_BEGIN
#if VR_REPROJECT == 1
    if (vtx_native_canvas.z > 0.5 &&
        (any(lessThan(vtx_native_canvas.xy, vec2(0.0))) ||
         any(greaterThan(vtx_native_canvas.xy, vec2(1.0))))) discard;
#endif
    // SS_NATIVE_CANVAS_CLIP_END
\t// Clip inside the box'''),
('\t\taddConstant("DIV_POS_Z", s->divPosZ);','\t\taddConstant("DIV_POS_Z", s->divPosZ);\n\t\taddConstant("VR_REPROJECT", s->divPosZ && config::VrReproject);')])
patch+=edit('core/rend/vr_reproject.cpp',[
('#include "dreamcast/dc_native_panel.h"','#include "dreamcast/dc_native_panel.h"\n#include "dreamcast/dc_native_menus.h"\n#include "dreamcast/dc_menu_presentation.h"\n#include "dreamcast/dc_mouse_native.h"'),
('    const bool active = config::VrReproject && !ctx.isRTT\n        &&(!silentScope||(submittedCamera.valid&&silentScopeWorldGeometry(ctx)));','''    bool nativeMenuRaster=false;
    if(silentScope && config::VrReproject && xr::currentEye()!=nullptr && !ctx.isRTT){
        silent_scope::dc_native_menus::NativeMenusProof menu;
        const bool current=menu.configure(settings.content.gameId,ctx);
        nativeMenuRaster=silent_scope::dc_menu::raster_menu(
            silent_scope::dc_mouse::native_menu_snapshot(),menu.kind(),menu.page(),
            menu.source_context(),current,true,ctx.isRTT);
    }
    const bool active = config::VrReproject && !ctx.isRTT && !nativeMenuRaster
        &&(!silentScope||(submittedCamera.valid&&silentScopeWorldGeometry(ctx)));''')])
(project/'patches/dreamcast/0030-native-menu-canvas.patch').write_text(patch,encoding='utf-8',newline='\n')
(backup/'receipt.json').write_text(json.dumps(entries,indent=2)+'\n')
print(json.dumps(entries,indent=2))
