"""Read bounded CVTRC001/2 native scene captures; never execute trace content."""
import struct
from pathlib import Path

HEADER=struct.Struct('<8sQIIIiiIII')
TRIANGLE=struct.Struct('<42d18I')
TEXTURE=struct.Struct('<QQII')
STATE_NAMES=('color_path','alpha_mode','depth_mode','fog_mode','texture_mode',
             'color0','color1','fog_color','chroma_key','texture_mode1','alpha_reference',
             'depth_constant','clip_left','clip_right','clip_top','clip_bottom')

def read_trace(path):
    path=Path(path)
    if path.stat().st_size>128*1024*1024: raise ValueError('Trace exceeds the memory budget')
    with path.open('rb') as stream:
        def read(n):
            data=stream.read(n)
            if len(data)!=n: raise ValueError('Truncated trace')
            return data
        magic,seq,w,h,color,xoff,yoff,complete,ntri,ntex=HEADER.unpack(read(HEADER.size))
        if magic not in (b'CVTRC001',b'CVTRC002',b'CVTRC003',b'CVTRC004',b'CVTRC005'): raise ValueError('Unknown trace version')
        if not 0<w<=8192 or not 0<h<=8192 or ntri>32768 or ntex>ntri*2 or complete not in (0,1):
            raise ValueError('Invalid trace dimensions or counts')
        background=b'';background_depth=b''
        if magic in (b'CVTRC003',b'CVTRC004',b'CVTRC005'):
            count=struct.unpack('<I',read(4))[0]
            if count not in (0,w*h): raise ValueError('Invalid background size')
            background=read(count*4)
        if magic in (b'CVTRC004',b'CVTRC005'):
            count=struct.unpack('<I',read(4))[0]
            if count not in (0,w*h) or (count and not background): raise ValueError('Invalid background depth size')
            background_depth=read(count*2)
        triangles=[]
        for _ in range(ntri):
            fields=TRIANGLE.unpack(read(TRIANGLE.size))
            textures=tuple(None if index==0xffffffff else index for index in fields[58:60])
            if any(index is not None and index>=ntex for index in textures):
                raise ValueError('Texture reference outside frame')
            fog=struct.unpack('<64HII',read(136)) if magic==b'CVTRC005' else (0,)*64+(255,0)
            triangles.append({'fog_table':fog[:64],'fog_delta_mask':fog[64],'overlay':bool(fog[65]),'vertices':tuple(fields[i:i+14] for i in (0,14,28)),
                              'state':dict(zip(STATE_NAMES,fields[42:58])), 'textures':textures})
        textures=[]
        total=0
        for _ in range(ntex):
            id_,generation,tw,th=TEXTURE.unpack(read(TEXTURE.size))
            lod=struct.unpack('<I',read(4))[0] if magic in (b'CVTRC002',b'CVTRC003',b'CVTRC004',b'CVTRC005') else 0
            if lod>8: raise ValueError('Invalid texture LOD')
            if not 0<tw<=4096 or not 0<th<=4096: raise ValueError('Invalid texture dimensions')
            total+=tw*th*4
            if total>64*1024*1024: raise ValueError('Texture data exceeds frame budget')
            textures.append({'id':id_,'generation':generation,'width':tw,'height':th,'lod':lod,'pixels':read(tw*th*4)})
        if stream.read(1): raise ValueError('Unexpected data after trace')
        return {'sequence':seq,'width':w,'height':h,'clear_color':color,'xoff':xoff,'yoff':yoff,
                'complete':bool(complete),'background':background,'background_depth':background_depth,'triangles':triangles,'textures':textures}
