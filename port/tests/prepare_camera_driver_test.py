import importlib.util
from pathlib import Path
import hashlib
import re
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('prepare_engine',root/'tools/prepare-engine.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
assert callable(getattr(module,'driver_source',None)), 'Driver integration must be reviewable without compiler-tree writes'
# Read-only filesystem access is available while external git metadata is restricted.
# Reverse the exact existing bridge in memory, then verify pinned original blob identity.
lines=Path('D:/VR4ALL-SilentScope-Build/mame-source/src/mame/konami/hornet.cpp').read_text().splitlines(keepends=True)
patch=(root/'patches/0001-silent-scope-bridge.patch').read_text().splitlines(keepends=True)
hunks=[]
for line in patch:
 if line.startswith('@@'):
  match=re.match(r'@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@',line);hunks.append((int(match[1])-1,[],[]))
 elif hunks and line[:1] in (' ','+','-'):
  if line[0] in (' ','+'):hunks[-1][1].append(line[1:])
  if line[0] in (' ','-'):hunks[-1][2].append(line[1:])
for start,new,old in reversed(hunks):
 assert lines[start:start+len(new)]==new, 'Existing driver differs from recorded bridge'
 lines[start:start+len(new)]=old
original=''.join(lines)
blob=original.encode();digest=hashlib.sha1(b'blob '+str(len(blob)).encode()+b'\0'+blob).hexdigest()
assert digest.startswith('39d55b68'), 'Reconstructed source must match pinned original blob'

source=module.driver_source(original)
assert source.count('silent_scope_camera_setup();')==2 # declaration and start call
assert 'install_write_tap' in source and 'm_dsp[0]->state_int(STATE_GENPC)' in source
assert 'pc==0x20153' in source and 'pc==0x20176' in source
assert 'offset==address?value:' in source and 'if(a==address)value=word;' in source
assert 'return m_vr_camera_state;' in source, 'Per-triangle provider must copy cached state without DSP reads'
assert 'm_vr_wider_code_taps' in source and 'space(AS_PROGRAM).install_write_tap' in source, 'Guarded PM code rewrites invalidate cached camera immediately'
assert 'm_vr_wider_code_data_taps' in source and 'program_data_aliases' in source, 'DM aliases of guarded program words also invalidate camera cache'
assert 'SILENT_SCOPE_WIDER_BEGIN_NATIVE' in source and 'SILENT_SCOPE_WIDER_END_NATIVE' in source, 'Controlled native request window is opt-in'
assert 'SILENT_SCOPE_CAMERA_DIAGNOSTIC' in source and 'silent_scope_camera_diagnostic' in source, 'Bounded camera eligibility diagnostics are opt-in'
assert '#include "silent_scope/operator_service.h"' in source and 'read_operator_state' in source, 'Camera eligibility observes actual native operator state'
assert source.index('const auto operator_state=')<source.index('silent_scope_camera_request();',source.index('void hornet_state::silent_scope_camera_sync()')), 'Native operator state must be refreshed before camera request/eligibility'
assert 'OperatorState::Normal' in source, 'Unknown native operator state keeps safe original presentation'
assert 'set_native_camera_provider' in source and 'clear_native_camera_provider' in source
assert 'm_dsp[1]->space(AS_DATA).install_write_tap' not in source
assert 'm_dsp[0]->enable_fast_sram();m_dsp[1]->enable_fast_sram();' in source

poll=source[source.index('void hornet_state::silent_scope_camera_sync()'):source.index('void hornet_state::silent_scope_camera_setup()')]
assert '.write_dword(' not in poll and '.synchronize' not in poll, 'OSD/screen poll must not alter a camera mid-projection'
eligibility=source[source.index('bool hornet_state::silent_scope_camera_gameplay()'):source.index('silent_scope::scene::WiderViewState hornet_state::silent_scope_camera_state()')]
assert 'pause_requested' not in eligibility, 'Opening VR menu freezes the actual native matrix; it must not restore FOV'
assert source.count('m_vr_wider.synchronize_at_setup(')==1, 'Only the validated staged completion writes camera outputs'
assert 'silent_scope_camera_diagnostic("staged_restore")' in source, 'Exact native Original restore has a staged diagnostic receipt'


field_tap=source[source.index('"silent-scope-main-wider"'):source.index('// The20 instruction words')]
assert 'intercept_write_cached' in field_tap and 'const u32 old_word=' in field_tap, 'Effective native camera bits determine cache invalidation after interception'
assert 'm_vr_camera_state={}; // A camera builder may be between fields.' not in field_tap, 'Identical native camera rewrites must preserve valid actual metadata'


diagnostic=source[source.index('void hornet_state::silent_scope_camera_diagnostic('):source.index('void hornet_state::silent_scope_camera_sync()')]
assert 'SILENT_SCOPE_CAMERA_DIAGNOSTIC_BEGIN_NATIVE' in diagnostic and 'SILENT_SCOPE_CAMERA_DIAGNOSTIC_END_NATIVE' in diagnostic, 'Diagnostic window must cover actual application and restore'
assert 'm_vr_camera_diag_last[category]==second' in diagnostic, 'Routine/issue categories are sampled at most once per native second'
assert 'm_vr_camera_diag_count>=256' in diagnostic and 'm_vr_camera_diag_count>=192' in diagnostic and 'priority' in diagnostic, 'Priority apply/restore receipts retain a reserved bounded budget'
assert 'staged_apply' in diagnostic and 'staged_restore' in diagnostic, 'Apply/restore bypass normal staged spam cadence'
assert '.write_dword(' not in diagnostic, 'Diagnostic throttling cannot touch native game state'

print('Pinned driver generation, effective cache, safe setup, diagnostic cadence/reserved receipts and capture passed')
