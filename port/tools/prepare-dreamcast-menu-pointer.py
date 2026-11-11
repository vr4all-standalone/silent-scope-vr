"""Apply the narrow XR pointer integration to the accepted0.0.7 engine.

Preserves the surrounding production changes and retains an exact before/after
patch. Native mouse policy and geometric proof are staged separately.
"""
from pathlib import Path
import difflib
import json
import hashlib

project = Path(__file__).resolve().parents[1]
source = Path('D:/VR4ALL-SilentScope-Build/flycast-source')
target = source / 'core/rend/vr/xr_host.cpp'
before = target.read_bytes()
text = before.decode('utf-8')

def replace_once(old, new):
    global text
    if text.count(old) != 1:
        raise SystemExit(f'Expected exactly one current production anchor: {old[:100]}')
    text = text.replace(old, new, 1)

replace_once(
    '    static auto previousInputDomain=ss::dc_ui::Domain::Unknown;\n'
    '    if(inputDomain!=previousInputDomain){\n'
    '        previousInputDomain=inputDomain;',
    '    ss::dc_native_menu::MainMenuProof mainMenuProof;\n'
    '    const bool mainMenuVerified=inputDomain==ss::dc_ui::Domain::NativeUi&&ctx&&\n'
    '        mainMenuProof.configure(settings.content.gameId,*ctx);\n'
    '    static auto previousInputDomain=ss::dc_ui::Domain::Unknown;\n'
    '    static bool previousMainMenu{};\n'
    '    if(inputDomain!=previousInputDomain||mainMenuVerified!=previousMainMenu){\n'
    '        previousInputDomain=inputDomain;previousMainMenu=mainMenuVerified;')
replace_once('    target.native_ui=nativeUi;',
    '    target.native_ui=nativeUi;target.ui_main_menu=mainMenuVerified;')
replace_once(
    '            if(target.native_ui){native=o+d*dist;target.point_valid=hit&&dist>0;target.ui_select=pull;}',
    '            if(target.native_ui){\n'
    '                native=o+d*dist;target.point_valid=hit&&dist>0;target.ui_select=pull;\n'
    '                // Pixel bounds come from the verified native glyph mesh.\n'
    '                // A miss remains Main Menu, never an unpositioned click.\n'
    '                if(target.ui_main_menu&&target.point_valid)\n'
    '                    target.ui_pointer_item=mainMenuProof.hit(*ctx,screen.x*640.f,screen.y*480.f);\n'
    '            }')
replace_once(
    '            // Only a verified flat native UI frame permits an unpositioned\n'
    '            // confirmation click. Unknown/unowned/Original3D is never UI.',
    '            // Generic verified flat notices retain their confirmation.\n'
    '            // Main Menu additionally requires a verified item hit and ACK.')
replace_once('"Either stick Up/Down: Native menus"','"Main menu: Point + Trigger"')
replace_once('"Aim smoothing: VR settings"','"Aim smoothing: VR Menu (- / +)"')

patch = ''.join(difflib.unified_diff(before.decode('utf-8').splitlines(True),text.splitlines(True),
    fromfile='a/core/rend/vr/xr_host.cpp',tofile='b/core/rend/vr/xr_host.cpp'))
evidence = project / 'private/flycast-research/menu-pointer-before'
evidence.mkdir(exist_ok=True)
backup = evidence / 'xr_host.cpp'
if backup.exists():
    raise SystemExit('Preserve existing menu-pointer baseline; this script is intentionally one-shot')
backup.write_bytes(before)
target.write_bytes(text.encode('utf-8'))
patch_path=project/'patches/dreamcast/0026-xr-native-menu-pointer.patch'
patch_path.parent.mkdir(parents=True,exist_ok=True)
patch_path.write_text(patch,encoding='utf-8',newline='\n')
(evidence/'receipt.json').write_text(json.dumps({
    'file':str(target),'beforeSha256':hashlib.sha256(before).hexdigest(),
    'afterSha256':hashlib.sha256(text.encode('utf-8')).hexdigest(),
    'patch':str(patch_path)},indent=2)+'\n',encoding='utf-8')
print(patch_path)
