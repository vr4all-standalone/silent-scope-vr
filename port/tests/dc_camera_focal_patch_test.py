from pathlib import Path
import re,struct
project=Path(__file__).resolve().parents[1]
patch=(project/'private/flycast-research/dc-camera-focal-setter-probe.patch').read_text()
added='\n'.join(s[1:]for s in patch.splitlines()if s.startswith('+')and not s.startswith('+++'))
assert 'handle_camera_focal_trap(ctx,pc)' in added
assert 'handle_camera_widen_trap(ctx,pc)' not in added
assert 'now<43||cameraFocalOwned' in added
assert 'OpPtr[FocalSetterProbe::OriginalOpcode](ctx,FocalSetterProbe::OriginalOpcode)' in added
assert 'OpPtr[FocalSetterProbe::StoreOpcode](ctx,FocalSetterProbe::StoreOpcode)' in added
assert 'CameraFocalRegisterRestore restore(ctx,decision.effective_bits)' in added
assert 'AdjustDelaySlotException(ex)' in added
assert 'ctx->pc=FocalSetterProbe::TargetPc+6' in added and 'ctx->pc=returnPc' in added
assert 'if(decision.eligible)' in added and 'const u32 returnPc=ctx->pr;' in added
assert 'WriteMem32' not in added and 'WriteMem8' not in added
assert added.count('WriteMem16_nommu(')==2
header=(project/'native/dreamcast/dc_camera_focal_probe.h').read_text()
boot=(project/'private/flycast-research/camera-1st-read.bin').read_bytes()
for address,word in re.findall(r'\{(0x8c[\da-f]+),(0x[\da-f]+)\}',header):
    address,word=int(address,16),int(word,16)
    fmt='<H'if word<=0xffff else'<I'
    assert struct.unpack_from(fmt,boot,address-0x8c010000)[0]==word,(hex(address),hex(word))
assert struct.unpack_from('<H',boot,0x8c112854-0x8c010000)[0]==0xd201
core=Path('D:/VR4ALL-SilentScope-Build/flycast-source/core')
ops=(core/'hw/sh4/interpr/sh4_opcodes.cpp').read_text()
assert 'Sh4Interpreter::Instance->ExecuteDelayslot()' in ops
assert 'u32 newpc = ctx->pr;\n\texecuteDelaySlot()' in ops
assert '(disp << 2) + ((ctx->pc + 2) & 0xFFFFFFFC)' in ops
assert 'ex.epc -= 2;' in(core/'hw/sh4/sh4_core.h').read_text()
assert 'Sh4Ex_SlotFpuDisabled' in(core/'hw/sh4/sh4_core.h').read_text()
print('Actual focal setter/caller bytes, original MOV/FM0V replay, RAII FR4 restoration, native RTS fallback and checkpoint43 boundary passed')
