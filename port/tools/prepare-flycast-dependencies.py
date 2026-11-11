"""Populate only the isolated prototype's exact, locally available dependencies."""
import argparse
import json
from pathlib import Path
import subprocess


def git(*arguments):
    return subprocess.check_output(['git', *map(str, arguments)], text=True).strip()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('source', type=Path)
    args=parser.parse_args()
    root=args.source.resolve()
    expected=Path('D:/VR4ALL-SilentScope-Build/flycast-source').resolve()
    if root!=expected:
        raise SystemExit('Only the dedicated Silent Scope prototype is writable')
    manifest=json.loads(args.manifest.read_text())
    if git('-C',root,'rev-parse','HEAD')!=manifest['baseline']:
        raise SystemExit('Prototype baseline differs from audited release')
    receipts=[]
    for item in sorted(manifest['gitlinks'],key=lambda row:(row['path'].count('/'),row['path'])):
        relative=Path(item['path'])
        target=(root/relative).resolve()
        if relative.is_absolute() or '..' in relative.parts or not target.is_relative_to(root):
            raise SystemExit('Dependency target leaves isolated source')
        commit=item['required_commit'];local=Path(item['local_git_dir']).resolve()
        if not local.is_relative_to(Path(manifest['root']).resolve()):
            raise SystemExit('Unrecognized local dependency object store')
        git('--git-dir',local,'cat-file','-e',commit+'^{commit}')
        if not (target/'.git').exists():
            if target.exists() and any(target.iterdir()):
                raise SystemExit('Preserve unexpected existing dependency: '+str(target))
            # Shared immutable Git objects, never another mod's working files.
            # No fetch, network URL, reset, clean or source-tree overwrite.
            subprocess.run(['git','clone','--local','--shared','--no-checkout',str(local),str(target)],check=True)
            subprocess.run(['git','-C',str(target),'-c','core.symlinks=false','checkout','--detach',commit],check=True)
        actual=git('-C',target,'rev-parse','HEAD')
        if actual!=commit:
            raise SystemExit('Existing dependency differs; refusing overwrite: '+str(target))
        receipts.append({'path':item['path'],'revision':actual,'local_object_store':str(local)})
    print(json.dumps({'baseline':manifest['baseline'],'dependencies':receipts},indent=2))


if __name__=='__main__':
    main()
