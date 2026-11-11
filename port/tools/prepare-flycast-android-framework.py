"""Prepare only pinned engine Java framework files; never copy game launchers."""
from pathlib import Path
import hashlib,json,subprocess,sys

def main():
    if len(sys.argv)!=2:raise SystemExit('Supply a new D:/VR4ALL-SilentScope-Build/android-framework-N directory')
    source=Path('D:/VR4ALL-SilentScope-Build/flycast-source')
    out=Path(sys.argv[1]).resolve()
    root=Path('D:/VR4ALL-SilentScope-Build').resolve()
    if out.parent!=root or not out.name.startswith('android-framework-') or out.exists():raise SystemExit('Use a new direct private build-root child')
    revision=subprocess.check_output(['git','--no-optional-locks','-C',str(source),'rev-parse','HEAD'],text=True).strip()
    if revision!='8398fead532ede9d52c37addef3353611b6c5ce7':raise SystemExit('Unexpected engine base')
    project=Path(__file__).resolve().parents[1]
    base=source/'shell/android-studio/flycast/src/main/java'
    own=project/'android-dreamcast/app/src/main/java'
    excluded={'BaseGLActivity.java','ReadyLaunchPolicy.java','VrSetupActivity.java','VrGames.java'}
    selected=[]
    for file in sorted(base.rglob('*.java')):
        if file.name.startswith('Cm') or file.name in excluded:continue
        relative=file.relative_to(base)
        if (own/relative).exists():raise SystemExit(f'Framework collides with adapter: {relative}')
        if file.is_symlink():raise SystemExit('Unexpected framework symlink')
        selected.append((file,relative))
    out.mkdir()
    receipts=[]
    for file,relative in selected:
        data=file.read_bytes();target=out/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        receipts.append({'source':str(file),'relative':str(relative),'sha256':hashlib.sha256(data).hexdigest()})
    (out/'framework-manifest.json').write_text(json.dumps({'revision':revision,'files':receipts},indent=2))
    print(f'{len(receipts)} pinned Java framework files prepared; custom launchers excluded')

if __name__=='__main__':main()
