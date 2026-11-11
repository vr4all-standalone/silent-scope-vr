from pathlib import Path
import re,struct
project=Path(__file__).resolve().parents[1]
patch=(project/'private/flycast-research/dc-camera-main-load-probe.patch').read_text()
assert 'core/reios/reios.cpp' in patch and 'core/rend/vr_reproject.cpp' in patch
assert 'handle_camera_widen_trap(ctx,pc)' in patch
assert patch.index('handle_camera_widen_trap(ctx,pc)') < patch.index('auto it = hooks.find(mapd)')
assert 'OpPtr[CameraWidenProbe::OriginalOpcode](ctx,CameraWidenProbe::OriginalOpcode);' in patch
assert 'throw SH4ThrownException(guestPc,Sh4Ex_FpuDisabled)' in patch
assert 'ctx->fr_hex(3)=decision.effective_bits' in patch
assert 'ctx->pc=' not in patch and 'ctx->pc =' not in patch
assert 'ctx->r[3]=' not in patch and 'ctx->fpscr.full=' not in patch
assert 'bm_RamWriteAccess(CameraWidenProbe::TargetPc);' in patch
assert 'icache.Invalidate();' in patch
assert 'WriteMem32' not in patch and 'WriteMem8' not in patch
assert not re.search(r'WriteMem16_nommu\([^C]',patch)
assert 'ensureSilentScopeCameraTrap(ctx.isRTT,now);' in patch
assert 'if(!cameraWidenProbe().enabled()&&!cameraTrapOwned)return;' in patch
assert 'if(ctx.dcCamera.supported)ctx.dcCamera.native_seconds=now;' in patch
assert 'environment_camera_geometry_probe().record(ctx);' in patch
assert 'SILENT_SCOPE_DC_CAMERA_WIDEN_PROBE' in patch
header=(project/'native/dreamcast/dc_camera_widen_probe.h').read_text()
boot=(project/'private/flycast-research/camera-1st-read.bin').read_bytes()
for address,word in re.findall(r'\{(0x8c[\da-f]+),(0x[\da-f]+)\}',header):
    address,word=int(address,16),int(word,16)
    assert struct.unpack_from('<H',boot,address-0x8c010000)[0]==word,(hex(address),hex(word))
assert struct.unpack_from('<H',boot,0x8c019002-0x8c010000)[0]==0xf338
source=Path('D:/VR4ALL-SilentScope-Build/flycast-source/core')
regalloc=(source/'hw/sh4/dyna/ssa_regalloc.h').read_text()
assert 'if (op->op == shop_ifb)\n\t\t{\n\t\t\tFlushAllRegs(true);' in regalloc
decoder=(source/'hw/sh4/dyna/decoder.cpp').read_text()
assert 'opcd.rs2 = shil_param(state.cpu.rpc + 2);' in decoder
assert 'dec_DynamicSet(reg_nextpc);' in decoder
fpu=(source/'hw/sh4/interpr/sh4_fpu.cpp').read_text()
assert 'ctx->fr_hex(n) = ReadMem32(ctx->r[m]);' in fpu
assert 'ctx->xd_hex(n) = ReadMem64(ctx->r[m]);' in fpu
print('Actual boot guards, original FMOV replay, dynamic continuation, DRC spills and bounded code-only trap writes verified')
