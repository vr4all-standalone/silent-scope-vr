"""Read-only owned-media audit; legacy timekeepers are diagnostics, never exact matches."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import zlib


def _entries(archive):
    infos = [item for item in archive.infolist() if not item.is_dir()]
    names = [item.filename.casefold() for item in infos]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate archive filenames')
    if len(infos) > 4096 or sum(item.file_size for item in infos) > 256 * 1024 * 1024:
        raise ValueError('Archive exceeds bounded arcade set limits')
    return infos


def _matches(archive, infos, expected):
    for item in infos:
        if item.file_size != expected['size'] or item.CRC != int(expected['crc'], 16):
            continue
        data = archive.read(item)
        if hashlib.sha1(data).hexdigest() == expected['sha1']:
            return item
    return None


def audit_archive(path, manifest):
    path = Path(path)
    with path.open('rb') as source:
        archive_sha256 = hashlib.file_digest(source, 'sha256').hexdigest()
    result = {'path': str(path.resolve()), 'sha256': archive_sha256,
              'matched': [], 'missing': [], 'mismatched': [], 'legacyNvram': []}
    with zipfile.ZipFile(path) as archive:
        infos = _entries(archive)
        for expected in manifest['roms']:
            item = _matches(archive, infos, expected)
            if item:
                result['matched'].append({**expected, 'archiveName': item.filename})
            elif any(i.filename.casefold() == expected['name'].casefold() for i in infos):
                result['mismatched'].append(expected)
            else:
                result['missing'].append(expected['name'])
        for expected in manifest.get('legacyNvram', []):
            item = _matches(archive, infos, expected)
            if item:
                result['legacyNvram'].append({**expected, 'archiveName': item.filename})
    result['exact'] = not result['missing'] and not result['mismatched']
    return result


def prepare_diagnostic_nvram(path, output, candidate, manifest):
    result = audit_archive(path, manifest)
    found = next((entry for entry in result['legacyNvram'] if entry['archiveName'] == candidate), None)
    if found is None:
        raise ValueError('Diagnostic candidate must match a recognized supplied legacy timekeeper')
    output = Path(output)
    expected_name = manifest['nvramName']
    if Path(expected_name).name != expected_name:
        raise ValueError('Invalid manifest timekeeper filename')
    if output.resolve() == Path(path).resolve() or output.exists():
        raise ValueError('Use a new private diagnostic directory; existing output is preserved')
    with zipfile.ZipFile(path) as archive:
        data = archive.read(candidate)
    output.mkdir(parents=True, exist_ok=False)
    override = output / expected_name
    override.write_bytes(data)
    (output / 'DIAGNOSTIC-ONLY.json').write_text(json.dumps({
        'status': 'legacy-timekeeper-substitution-not-an-exact-rom-match',
        'sourceArchive': str(Path(path).resolve()), 'sourceArchiveSha256': result['sha256'],
        'sourceEntry': found, 'overrideName': expected_name,
        'crc': f'{zlib.crc32(data):08x}', 'sha1': hashlib.sha1(data).hexdigest(),
    }, indent=2) + '\n', encoding='utf-8')
    return override


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).parents[1] / 'game-sets.json')
    parser.add_argument('--diagnostic-output', type=Path)
    parser.add_argument('--candidate')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    if args.diagnostic_output:
        if not args.candidate:
            parser.error('--diagnostic-output requires --candidate')
        print(prepare_diagnostic_nvram(args.archive, args.diagnostic_output, args.candidate, manifest))
        return 0
    result = audit_archive(args.archive, manifest)
    print(json.dumps(result, indent=2))
    return 0 if result['exact'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
