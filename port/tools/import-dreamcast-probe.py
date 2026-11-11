#!/usr/bin/env python3
"""Import the user's known Silent Scope image into a new private probe directory.

This tool does not launch an emulator or modify the source archive. Failed output
is retained deliberately; choose a different new directory for every attempt.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import zlib


PROJECT = Path(__file__).resolve().parents[1]
ARCHIVE = Path('E:/___flattovrdev/confidential mission/Roms/Silent Scope (USA).7z')
SEVEN_ZIP = Path('C:/Program Files/7-Zip/7z.exe')
METADATA = PROJECT / 'private/flycast-research/media-input-metadata.json'
OUTPUT_ROOTS = [Path('D:/VR4ALL-SilentScope-Build/diagnostics'),
                PROJECT / 'private/dreamcast-media']
MEMBERS = {
    'Silent Scope (USA) (Track 1).bin': {'size': 1058400, 'crc32': 'EBCA15F8'},
    'Silent Scope (USA) (Track 2).bin': {'size': 1589952, 'crc32': '8557FCAA'},
    'Silent Scope (USA) (Track 3).bin': {'size': 1185760800, 'crc32': '9460C4FE'},
    'Silent Scope (USA).cue': {'size': 348, 'crc32': 'A35070E4'},
}
CUE_SHA256 = '6e3b5cf89c4cf244900d59fc270de6578794aab1d0b570d11e0f2faa54d9e68b'
PREFIX_SHA256 = '469534ce03c525b1b3b61ea01bb8bae33806e48de71c6c86ceeeba70983ce137'
MARGIN = 256 * 1024 * 1024
RECEIPT = 'import-verification.json'


def safe_basename(name):
    return (isinstance(name, str) and bool(name) and name not in ('.', '..')
            and not any(c in name for c in '/\\:*?"<>|\r\n\x00')
            and not name.endswith((' ', '.')))


def below(path, root):
    return path != root and root in path.parents


def check_destination(output, roots):
    output = Path(output)
    if os.path.lexists(output):
        raise ValueError('Output already exists; it will not be reused or overwritten')
    resolved = output.resolve()
    if not any(below(resolved, Path(root).resolve()) for root in roots):
        raise ValueError('Output must be strictly inside an approved private root')
    return resolved


def parse_archive_listing(text, expected):
    if len(text) > 1024 * 1024 or '\x00' in text or '\n----------\n' not in text.replace('\r\n', '\n'):
        raise ValueError('Invalid or oversized 7-Zip structured listing')
    header, members = text.replace('\r\n', '\n').split('\n----------\n', 1)

    def fields(block):
        result = {}
        for line in block.splitlines():
            if ' = ' not in line:
                continue  # Banner and warning text are not member properties.
            key, value = line.split(' = ', 1)
            if key in result:
                raise ValueError('Duplicate structured archive property')
            result[key] = value
        return result

    info = fields(header)
    if info.get('Type') != '7z':
        raise ValueError('Expected a 7z archive')
    found = {}
    for block in re.split(r'\n\s*\n', members.strip()):
        item = fields(block)
        if not item:
            continue
        name = item.get('Path')
        if not safe_basename(name) or name not in expected or name.casefold() in found:
            raise ValueError('Unexpected, unsafe or duplicate archive member')
        if (item.get('Encrypted', '-') != '-' or item.get('Folder', '-') != '-'
                or item.get('Symbolic Link') or item.get('Hard Link')
                or 'D' in item.get('Attributes', '') or 'L' in item.get('Attributes', '')):
            raise ValueError('Encrypted or non-regular archive member')
        try:
            size = int(item['Size'])
            crc = item['CRC'].upper()
        except (KeyError, ValueError):
            raise ValueError('Archive member has no valid size or CRC') from None
        if size != expected[name]['size'] or crc != expected[name]['crc32']:
            raise ValueError('Archive member does not match the known profile')
        found[name.casefold()] = name
    if set(found.values()) != set(expected):
        raise ValueError('Archive member set is incomplete')
    try:
        tail = int(info.get('Tail Size', '0'))
        physical = int(info['Physical Size'])
    except (KeyError, ValueError):
        raise ValueError('Invalid archive physical or tail size') from None
    if tail < 0 or physical <= 0:
        raise ValueError('Invalid archive sizes')
    return {'tail_bytes': tail, 'physical_bytes': physical}


def hash_file(path):
    sha = hashlib.sha256()
    crc = 0
    count = 0
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            sha.update(chunk)
            crc = zlib.crc32(chunk, crc)
            count += len(chunk)
    return {'size': count, 'crc32': f'{crc:08X}', 'sha256': sha.hexdigest()}


def verify_members(output, expected):
    actual = list(output.iterdir())
    if {item.name for item in actual} != set(expected):
        raise ValueError('Extracted member set is incomplete or unexpected')
    results = {}
    for item in actual:
        status = item.lstat()
        if (not stat.S_ISREG(status.st_mode) or item.is_symlink()
                or getattr(status, 'st_file_attributes', 0) & 0x400 or status.st_nlink != 1):
            raise ValueError('Extracted member is not an independent regular file')
        if status.st_size != expected[item.name]['size']:
            raise ValueError('Extracted file size does not match the known profile')
        result = hash_file(item)
        if result['size'] != expected[item.name]['size'] or result['crc32'] != expected[item.name]['crc32']:
            raise ValueError('Extracted file CRC does not match the known profile')
        results[item.name] = result
    return results


def run_seven_zip(args):
    return subprocess.run([str(arg) for arg in args], shell=False, capture_output=True,
                          text=True, encoding='utf-8', errors='replace', timeout=1800)


def extract_and_verify(archive, output, seven_zip, expected, roots, *, runner=run_seven_zip,
                       free_bytes=None, margin=MARGIN, validators=()):
    archive = Path(archive).resolve()
    output = check_destination(output, roots)
    if not archive.is_file() or archive.is_symlink():
        raise ValueError('Source archive must be an existing regular file')
    if not expected or any(not safe_basename(name) for name in expected):
        raise ValueError('Invalid member allowlist')
    listing = runner([str(seven_zip), 'l', '-slt', '-bsp0', '-bse2', '--', str(archive)])
    if listing.returncode not in (0, 1):
        raise ValueError('7-Zip could not list the archive')
    info = parse_archive_listing(listing.stdout, expected)
    required = sum(item['size'] for item in expected.values()) + margin
    ancestor = output.parent
    while not ancestor.exists():
        ancestor = ancestor.parent
    available = shutil.disk_usage(ancestor).free if free_bytes is None else free_bytes
    if margin < 0 or available < required:
        raise ValueError('Insufficient free space for the complete image plus safety margin')
    report = {'status': 'failed', 'output': str(output),
              'archive': {'path': str(archive), **info, **hash_file(archive)},
              'seven_zip': {'listing_exit': listing.returncode},
              'required_free_bytes': required, 'available_free_bytes': available}
    # A new directory and -aos independently prevent media overwrite. No cleanup
    # is attempted after this point, including extraction failure or interruption.
    output.parent.mkdir(parents=True, exist_ok=True)
    check_destination(output, roots)
    output.mkdir(exist_ok=False)
    try:
        extraction = runner([str(seven_zip), 'e', '-y', '-aos', '-spd', '-bsp0', '-bse2',
                             '-o' + str(output), '--', str(archive), *expected])
        report['seven_zip']['extraction_exit'] = extraction.returncode
        report['seven_zip']['warning_exit_observed'] = 1 in (listing.returncode, extraction.returncode)
        if extraction.returncode not in (0, 1):
            raise ValueError('7-Zip reported a fatal extraction result')
        report['files'] = verify_members(output, expected)
        for validate in validators:
            validate(output)
        report['status'] = 'verified'
    except BaseException as error:
        # Persist only a category, never arbitrary subprocess stdout or ROM data.
        report['failure'] = type(error).__name__
        with (output / RECEIPT).open('x', encoding='utf-8') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
        raise ValueError('Import failed; partial output and private receipt were retained') from error
    with (output / RECEIPT).open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    return report


def parse_cue(data, allowed_members):
    if len(data) > 4096:
        raise ValueError('Oversized CUE')
    try:
        text = data.decode('ascii')
    except UnicodeDecodeError:
        raise ValueError('CUE must be ASCII') from None
    tracks, current_file, area, current = [], None, None, None
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line in ('REM SINGLE-DENSITY AREA', 'REM HIGH-DENSITY AREA'):
            area = line[4:]
            current_file, current = None, None
        elif match := re.fullmatch(r'FILE "([^"]+)" BINARY', line):
            name = match[1]
            if not safe_basename(name) or name not in allowed_members:
                raise ValueError('Unsafe or unexpected CUE file')
            current_file, current = name, None
        elif match := re.fullmatch(r'TRACK (\d{2}) (MODE1/2352|AUDIO)', line):
            number = int(match[1])
            if not current_file or not area or number < 1 or any(t['number'] == number for t in tracks):
                raise ValueError('Invalid CUE track sequence')
            current = {'number': number, 'file': current_file, 'area': area,
                       'mode': match[2], 'indexes': {}}
            tracks.append(current)
        elif match := re.fullmatch(r'INDEX (00|01) (\d{2}):(\d{2}):(\d{2})', line):
            index, minutes, seconds, frames = map(int, match.groups())
            if current is None or str(index) in current['indexes'] or seconds >= 60 or frames >= 75:
                raise ValueError('Invalid CUE index')
            current['indexes'][str(index)] = (minutes * 60 + seconds) * 75 + frames
        else:
            raise ValueError('Unexpected CUE directive')
    if not tracks or any('1' not in track['indexes'] for track in tracks):
        raise ValueError('CUE has no complete tracks')
    return tracks


def parse_ip_sector(sector):
    if len(sector) != 2352 or sector[:12] != bytes([0] + [255] * 10 + [0]) or sector[15] != 1:
        raise ValueError('Invalid raw Mode 1 boot sector')
    header = sector[16:272]
    def field(offset, size):
        try:
            value = header[offset:offset + size].decode('ascii')
        except UnicodeDecodeError:
            raise ValueError('Invalid ASCII IP metadata') from None
        if any(ord(c) < 32 or ord(c) > 126 for c in value):
            raise ValueError('Invalid IP metadata character')
        return value.rstrip()
    result = {name: field(offset, size) for name, offset, size in (
        ('hardware_id', 0, 16), ('maker_id', 16, 16), ('device_info', 32, 16),
        ('area_symbols', 48, 8), ('peripherals', 56, 7), ('product_number', 64, 10),
        ('product_version', 74, 6), ('release_date', 80, 16), ('boot_filename', 96, 16),
        ('software_company', 112, 16), ('software_name', 128, 128))}
    result['isWindowsCE'] = header[62] == ord('1') or result['boot_filename'].startswith('0WINCEOS.BIN')
    result['supportsVGA'] = header[61] == ord('1')
    return result


def load_profile(path):
    path = Path(path)
    if path.stat().st_size > 1024 * 1024:
        raise ValueError('Oversized metadata receipt')
    metadata = json.loads(path.read_text(encoding='utf-8'))
    tracks = metadata['cue']['tracks']
    if (Path(metadata['archive']).resolve() != ARCHIVE.resolve()
            or metadata['archive_bytes'] != 99751350 or metadata['cue']['bytes'] != 348
            or metadata['cue']['crc32'] != MEMBERS['Silent Scope (USA).cue']['crc32']
            or metadata['cue']['sha256'] != CUE_SHA256
            or metadata['sector_validation']['raw_prefix_sha256'] != PREFIX_SHA256
            or metadata['ip_meta']['product_number'] != 'T9507N'
            or metadata['ip_meta']['flycast_fields']['isWindowsCE'] is not False
            or len(tracks) != 3):
        raise ValueError('Metadata does not match the known owned-media receipt')
    for index, track in enumerate(tracks, 1):
        name = f'Silent Scope (USA) (Track {index}).bin'
        if track['number'] != index or track['file'] != name or track['file_bytes'] != MEMBERS[name]['size']:
            raise ValueError('Metadata track profile mismatch')
    return metadata


def game_validator(metadata):
    def validate(output):
        cue = output / 'Silent Scope (USA).cue'
        if hash_file(cue)['sha256'] != CUE_SHA256:
            raise ValueError('Known CUE hash mismatch')
        actual = parse_cue(cue.read_bytes(), set(MEMBERS) - {cue.name})
        expected = [{key: track[key] for key in ('number', 'file', 'area', 'mode', 'indexes')}
                    for track in metadata['cue']['tracks']]
        if actual != expected:
            raise ValueError('CUE layout mismatch')
        with (output / 'Silent Scope (USA) (Track 3).bin').open('rb') as stream:
            prefix = stream.read(64 * 2352)
        if hashlib.sha256(prefix).hexdigest() != PREFIX_SHA256:
            raise ValueError('Known boot-sector prefix hash mismatch')
        ip = parse_ip_sector(prefix[:2352])
        for name in ('hardware_id', 'maker_id', 'device_info', 'area_symbols', 'peripherals',
                     'product_number', 'product_version', 'release_date', 'boot_filename',
                     'software_company', 'software_name'):
            if ip[name] != metadata['ip_meta'][name]:
                raise ValueError('IP metadata mismatch')
        if ip['isWindowsCE'] or ip['product_number'] != 'T9507N':
            raise ValueError('Unexpected product or Windows CE boot')
    return validate


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--metadata', type=Path, default=METADATA)
    args = parser.parse_args(argv)
    try:
        if args.archive.resolve() != ARCHIVE.resolve() or args.archive.stat().st_size != 99751350:
            raise ValueError('Only the exact known user archive is accepted')
        if not SEVEN_ZIP.is_file() or SEVEN_ZIP.is_symlink():
            raise ValueError('The existing Program Files 7-Zip executable is required')
        metadata = load_profile(args.metadata)
        # Bind the actual archive header to the observed trailing-data warning.
        def production_runner(command):
            result = run_seven_zip(command)
            if command[1] == 'l' and result.returncode in (0, 1):
                listing = parse_archive_listing(result.stdout, MEMBERS)
                if listing != {'tail_bytes': 38, 'physical_bytes': 99751312}:
                    raise ValueError('Archive physical/tail profile mismatch; no repair attempted')
            return result
        report = extract_and_verify(args.archive, args.output, SEVEN_ZIP, MEMBERS, OUTPUT_ROOTS,
                                    runner=production_runner, validators=[game_validator(metadata)])
        print(json.dumps({'status': report['status'], 'receipt': str(Path(report['output']) / RECEIPT)}))
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print('Import rejected: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
