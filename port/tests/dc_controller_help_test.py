"""Exercise the production controller help with its bundled model and real font."""
from pathlib import Path
from zipfile import ZipFile
import json, os, re, subprocess, sys

source=Path(sys.argv[1])
output=Path(sys.argv[2])
output.mkdir(parents=True,exist_ok=True)
text=(source/'core/rend/vr/xr_labels.cpp').read_text()

def block(signature):
    start=text.index(signature)
    end=text.index('{',start)+1
    depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}')
        end+=1
    return text[start:end]

part=text[text.index('using controllers::Button;'):text.index('// Where each hand\'s label lines end')]
part+='\n'+block('void facing(')+'\n'+block('void layOut(')
model=(source/'core/rend/vr/controller_model.h').read_text()
anchors={}
for name,x,y,z in re.findall(r'\{ "(a|b|menu|stick|trigger|grip)", \d+u, \d+u, \{ ([^,]+), ([^,]+), ([^ }]+) \}',model):
    anchors[name]=[float(v.rstrip('f')) for v in (x,y,z)]
assert len(anchors)==6
names=['a','b','a','b','menu','stick','trigger','grip']
rows=[]
for hand in (0,1):
    values=[]
    for name in names:
        x,y,z=anchors[name]
        values.append('glm::vec3('+','.join(str(v)+'f' for v in ((-x if hand==0 else x),y,z))+')')
    rows.append('{'+','.join(values)+'}')
(output/'layout.inc').write_text(part+'\nconst glm::vec3 Anchors[2][8]={'+','.join(rows)+'};')
# The shipped reference GLBs use sidecar points in baked model coordinates.
# Their inverse aim node is180Y, matching xr_own.cpp/readController.
bundled=[]
models=Path(__file__).resolve().parents[1]/'assets/models/controllers'
for hand in ('left','right'):
    doc=json.loads((models/(hand+'.json')).read_text())
    assert doc['aim']['rotation']==[0,1,0,0] and doc['scale']==1
    names=(['x','y','x','y'] if hand=='left' else ['a','b','a','b'])+['menu','stick','trigger','grip']
    values=[]
    for name in names:
        point=doc['buttons'].get(name,[0,0,0])
        values.append('glm::vec3('+','.join(str(float(v)*(-1 if k in (0,2) else 1))+'f' for k,v in enumerate(point))+')')
    bundled.append('{'+','.join(values)+'}')
with (output/'layout.inc').open('a') as file:
    file.write('\nconst glm::vec3 BundledAnchors[2][8]={'+','.join(bundled)+'};')
with ZipFile(source/'fonts/Roboto-Bold.ttf.zip') as archive:
    (output/'Roboto-Bold.ttf').write_bytes(archive.read(archive.namelist()[0]))
compiler=Path('D:/msys64/mingw64/bin/g++.exe')
env=dict(os.environ)
env['PATH']=str(compiler.parent)+';'+env.get('PATH','')
command=[str(compiler),'-std=c++20','-O2','-static','-I'+str(output),
    '-I'+str(source/'core/deps'),'-I'+str(source/'core/deps/glm'),
    str(Path(__file__).with_suffix('.cpp')),'-o',str(output/'controller-help-test.exe')]
subprocess.run(command,check=True,env=env)
result=subprocess.run([str(output/'controller-help-test.exe'),str(output/'Roboto-Bold.ttf'),str(output/'controller-help-layout.json')],env=env)
sys.exit(result.returncode)
