from pathlib import Path
root=Path(__file__).resolve().parents[1]
def hunks(text):
 result=[]
 for line in text.splitlines(keepends=True):
  if line.startswith('@@'):result.append(([],[]))
  elif result and line[:1] in (' ','+','-'):
   if line[0] in (' ','-'):result[-1][0].append(line[1:])
   if line[0] in (' ','+'):result[-1][1].append(line[1:])
 return result
def apply(source,patch,reverse=False):
 lines=source.splitlines(keepends=True)
 for old,new in hunks(patch):
  if reverse:old,new=new,old
  hits=[i for i in range(len(lines)-len(old)+1) if lines[i:i+len(old)]==old]
  assert len(hits)==1, f'Pinned patch context must match exactly once: {old[:3]}'
  i=hits[0];lines[i:i+len(old)]=new
 return ''.join(lines)
source=Path('D:/VR4ALL-SilentScope-Build/mame-source/src/devices/video/voodoo.cpp').read_text()
patch=(root/'patches/0011-native-command-camera.patch').read_text()
if 'CameraCommandExecution capture_context' in source:source=apply(source,patch,True)
changed=apply(source,patch)
start=changed.index('void voodoo_1_device::map_register_w(')
end=changed.index('void voodoo_1_device::map_lfb_w(',start)
write=changed[start:end]
assert write.index('prepare_for_write()')<write.index('capture_accept_triangle')<write.index('if (pending &&'), 'Accept current command after flushing older work, before either FIFO/direct execution'
start=changed.index('s32 voodoo_1_device::triangle()')
end=changed.index('const char *voodoo_1_device::',start) if 'const char *voodoo_1_device::' in changed[start:] else len(changed)
triangle=changed[start:end]
assert triangle.index('capture_consume_triangle')<triangle.index('if (poly.destbase == nullptr)'), 'Even discarded native commands consume exactly one stamp'
assert 'CameraCommandExecution capture_context' in triangle
assert 'capture_device(tag())' in write and 'bool const capture_main=silent_scope::scene::capture_device(tag())' in triangle, 'Scope board never enters this metadata stream'
base_patch=(root/'patches/0005-main-scene-capture.patch').read_text().split('diff --git a/src/devices/video/voodoo_render.cpp')[0]
apply(changed,base_patch,True) #0011 must preserve idempotent verification of earlier0005.
assert 'capture_reset_commands(this)' in changed
hooks=(root/'native/scene/voodoo_hooks.h').read_text();body=hooks[hooks.index('void capture_triangle('):hooks.index('inline void capture_clear(')]
assert 'native_camera_state()' not in body and 'executing_command_camera()' in body, 'Rasterizer execution cannot query a later live camera'
print('Pinned FIFO acceptance/execution/reset patch, scope exclusion and0005 reverse compatibility passed')
