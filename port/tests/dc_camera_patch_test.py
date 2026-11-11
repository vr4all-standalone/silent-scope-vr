from pathlib import Path
import re,struct
root=Path(__file__).resolve().parents[1];engine=Path('D:/VR4ALL-SilentScope-Build/flycast-source')
patch=(root/'private/flycast-research/dc-camera-submission-probe.patch').read_text()
files={};name=None;hunks=[]
for line in patch.splitlines(keepends=True):
 if line.startswith('diff --git '):name=line.split()[2][2:];files[name]=[]
 elif line.startswith('@@'):files[name].append(([],[]))
 elif name and files[name] and line[:1] in (' ','+','-'):
  old,new=files[name][-1]
  if line[0] in (' ','-'):old.append(line[1:])
  if line[0] in (' ','+'):new.append(line[1:])
assert set(files)=={'core/rend/vr_reproject.h','core/rend/vr_reproject.cpp','core/hw/pvr/ta_ctx.h','core/hw/pvr/Renderer_if.cpp'}
changed={}
for name,hunks in files.items():
 source=(engine/name).read_text().splitlines(keepends=True)
 # Allow an already-applied coordinator copy without changing any engine file.
 forward=not any(marker in ''.join(source) for marker in ('stampSilentScopeCamera(ctx->rend','static silent_scope::dreamcast::CameraSampler','CameraContext dcCamera','void stampSilentScopeCamera(rend_context& ctx'))
 if not forward:
  for old,new in reversed(hunks):
   hits=[i for i in range(len(source)-len(new)+1)if source[i:i+len(new)]==new];assert len(hits)==1,(name,'reverse context')
   i=hits[0];source[i:i+len(new)]=old
 for old,new in hunks:
  hits=[i for i in range(len(source)-len(old)+1)if source[i:i+len(old)]==old];assert len(hits)==1,(name,'forward context')
  i=hits[0];source[i:i+len(old)]=new
 changed[name]=''.join(source)
renderer=changed['core/hw/pvr/Renderer_if.cpp'];start=renderer.index('void rend_start_render()');end=renderer.index('if (QueueRender(ctx))',start)
assert renderer.index('ctx->rend.isRTT =',start)<renderer.index('vr::stampSilentScopeCamera',start)<end
assert 'linked->rend.dcCamera=ctx->rend.dcCamera' in renderer
context=changed['core/hw/pvr/ta_ctx.h'];assert 'dcCamera={};' in context
adapter=changed['core/rend/vr_reproject.cpp'];snippet=adapter[adapter.index('// No native writes'):adapter.index('static ReprojectParams params;')]
assert 'WriteMem' not in snippet and 'ReadMem32_nommu' in snippet
for event in ('Start','LoadState','DiskChange','Terminate'):assert 'Event::'+event in snippet
# Native code guards must be derived exactly from the private owned boot image.
header=(root/'native/dreamcast/dc_camera_probe.h').read_text();boot=(root/'private/flycast-research/camera-1st-read.bin').read_bytes()
for address,value in re.findall(r'\{(0x8c[\da-f]+),(0x[\da-f]+)u\}',header):
 assert struct.unpack_from('<I',boot,int(address,16)-0x8c010000)[0]==int(value,16)
print('Pinned submission seam, context clearing/generation, no RAM writes/GL edits, and actual boot-code guards passed')
