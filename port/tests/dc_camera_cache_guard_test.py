from pathlib import Path
import re,struct
root=Path(__file__).resolve().parents[1]
header=(root/'native/dreamcast/dc_camera_probe.h').read_text()
boot=(root/'private/flycast-research/camera-1st-read.bin').read_bytes()
guards=re.findall(r'\{(0x8c[\da-f]+),(0x[\da-f]+)u\}',header)
assert len(guards)==24
for address,value in guards:
    assert struct.unpack_from('<I',boot,int(address,16)-0x8c010000)[0]==int(value,16),(address,value)
assert 'CacheBase=0x8cd206d4' in header and 'WriteMem' not in header
assert struct.unpack_from('<I',boot,0x8c111f60-0x8c010000)[0]==0x8cd206b0
assert struct.unpack_from('<I',boot,0x8c15f988-0x8c010000)[0]==0x8c14065c
# Refresh stores FX/FY/CX/CY atbase+24/+28/+2c/+30. Projector loads
# them sequentially after4headerwords, i.e.from base+24 throughbase+30.
assert struct.unpack_from('<4H',boot,0x8c14066c-0x8c010000)==(0xf049,0xf149,0xf249,0xf349)
assert struct.unpack_from('<2H',boot,0x8c111d0c-0x8c010000)==(0xf232,0xf427)
assert struct.unpack_from('<2H',boot,0x8c111d16-0x8c010000)==(0xf232,0xf427)
assert struct.unpack_from('<2H',boot,0x8c111d1e-0x8c010000)==(0xe02c,0xf437)
assert struct.unpack_from('<3H',boot,0x8c111d26-0x8c010000)==(0xe030,0x000b,0xf437)
print('All24 title-code guards and actual cachedFX/FY/CX/CY producer/consumer instruction bytes match owned boot image')
