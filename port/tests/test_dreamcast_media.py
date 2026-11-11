import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zlib


class DreamcastMediaTests(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).parents[1] / 'tools' / 'import-dreamcast-probe.py'
        self.assertTrue(path.exists(), 'Owned Dreamcast importer is missing')
        spec = importlib.util.spec_from_file_location('dreamcast_import', path)
        self.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.tool)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.allowed = self.root / 'allowed'
        self.allowed.mkdir()
        self.archive = self.root / 'owned.7z'
        self.archive.write_bytes(b'synthetic archive stand-in')
        self.payloads = {'track1.bin': b'one', 'track2.bin': b'two',
                         'track3.bin': b'three', 'game.cue': b'cue'}
        self.expected = {name: {'size': len(data), 'crc32': f'{zlib.crc32(data):08X}'}
                         for name, data in self.payloads.items()}

    def listing(self, entries=None):
        entries = entries if entries is not None else self.expected
        result = 'Type = 7z\nPhysical Size = 123\nTail Size = 38\n\n----------\n'
        for name, item in entries.items():
            result += f'Path = {name}\nSize = {item["size"]}\nCRC = {item["crc32"]}\nEncrypted = -\nAttributes = \n\n'
        return result

    def runner(self, extraction_code=0, changed=None):
        def run(args):
            if args[1] == 'l':
                return subprocess.CompletedProcess(args, 1, self.listing(), 'Trailing data warning')
            self.assertEqual(args[1], 'e')
            self.assertIn('-spd', args)
            self.assertIn('-aos', args)
            self.assertNotIn('-so', args)
            self.assertEqual(args[args.index('--') + 2:], list(self.expected))
            output = Path(next(value[2:] for value in args if value.startswith('-o')))
            for name, data in self.payloads.items():
                if changed == 'partial' and name == 'track3.bin':
                    continue
                (output / name).write_bytes(b'wrong' if name == changed else data)
            return subprocess.CompletedProcess(args, extraction_code, 'Extracted', 'Trailing data warning')
        return run

    def perform(self, output, **kwargs):
        return self.tool.extract_and_verify(self.archive, output, Path('7z.exe'), self.expected,
                                            [self.allowed], runner=self.runner(**kwargs),
                                            free_bytes=10000, margin=100)

    def test_warning_exit_is_accepted_only_after_real_file_crc_and_sha_verification(self):
        output = self.allowed / 'new'
        report = self.perform(output, extraction_code=1)
        self.assertEqual(report['status'], 'verified')
        self.assertEqual(report['archive']['tail_bytes'], 38)
        self.assertEqual(report['seven_zip']['extraction_exit'], 1)
        self.assertEqual(report['files']['track3.bin']['sha256'], hashlib.sha256(b'three').hexdigest())
        self.assertEqual(json.loads((output / 'import-verification.json').read_text())['status'], 'verified')

    def test_partial_and_wrong_content_never_report_success_and_are_preserved(self):
        for changed in ('partial', 'track1.bin'):
            output = self.allowed / changed
            with self.assertRaises(ValueError):
                self.perform(output, extraction_code=1, changed=changed)
            self.assertTrue(output.exists())
            self.assertTrue((output / 'track2.bin').exists())
            self.assertEqual(json.loads((output / 'import-verification.json').read_text())['status'], 'failed')

    def test_fatal_extraction_exit_is_rejected_even_if_files_happen_to_match(self):
        output = self.allowed / 'fatal'
        with self.assertRaises(ValueError):
            self.perform(output, extraction_code=2)
        self.assertEqual((output / 'track1.bin').read_bytes(), b'one')
        self.assertEqual(json.loads((output / 'import-verification.json').read_text())['status'], 'failed')

    def test_existing_output_and_allowlist_root_are_never_used(self):
        output = self.allowed / 'existing'
        output.mkdir()
        (output / 'keep').write_text('preserve')
        for candidate in (output, self.allowed, self.root / 'outside', self.root / 'allowed-suffix' / 'x'):
            with self.assertRaises(ValueError):
                self.perform(candidate)
        self.assertEqual((output / 'keep').read_text(), 'preserve')

    def test_symlink_escape_is_rejected(self):
        link = self.allowed / 'escape'
        try:
            link.symlink_to(self.root, target_is_directory=True)
        except OSError:
            self.skipTest('OS does not permit an unprivileged synthetic symlink')
        with self.assertRaises(ValueError):
            self.tool.check_destination(link / 'new', [self.allowed])

    def test_unexpected_duplicate_encrypted_and_changed_archive_members_fail_closed(self):
        variants = [self.listing({**self.expected, 'extra.bin': {'size': 1, 'crc32': '00000000'}}),
                    self.listing() + 'Path = track1.bin\nSize = 3\nCRC = 7A6C86F1\n\n',
                    self.listing().replace('Encrypted = -', 'Encrypted = +', 1),
                    self.listing().replace('\nSize = 3\n', '\nSize = 4\n', 1),
                    self.listing().replace('Path = track1.bin', 'Path = ../track1.bin', 1)]
        for listing in variants:
            with self.assertRaises(ValueError):
                self.tool.parse_archive_listing(listing, self.expected)

    def test_unexpected_output_and_hard_links_fail_before_success(self):
        for mode in ('unexpected', 'hardlink'):
            output = self.allowed / mode
            base = self.runner()
            def runner(args):
                result = base(args)
                if args[1] == 'e':
                    if mode == 'unexpected':
                        (output / 'extra.bin').write_bytes(b'extra')
                    else:
                        (self.root / 'linked.bin').hardlink_to(output / 'track1.bin')
                return result
            with self.assertRaises(ValueError):
                self.tool.extract_and_verify(self.archive, output, Path('7z.exe'), self.expected,
                                             [self.allowed], runner=runner, free_bytes=10000, margin=100)
            self.assertEqual(json.loads((output / 'import-verification.json').read_text())['status'], 'failed')

    def test_real_metadata_receipt_loads_without_opening_media(self):
        metadata = self.tool.load_profile(self.tool.METADATA)
        self.assertEqual(metadata['archive_bytes'], 99751350)
        self.assertEqual(sum(item['size'] for item in self.tool.MEMBERS.values()), 1188409500)

    def test_insufficient_space_prevents_creation_and_extraction(self):
        output = self.allowed / 'too-big'
        with self.assertRaises(ValueError):
            self.tool.extract_and_verify(self.archive, output, Path('7z.exe'), self.expected,
                                         [self.allowed], runner=self.runner(), free_bytes=1, margin=100)
        self.assertFalse(output.exists())

    def test_cue_paths_and_unknown_directives_are_rejected(self):
        valid = b'REM HIGH-DENSITY AREA\nFILE "track3.bin" BINARY\n TRACK 03 MODE1/2352\n INDEX 01 00:00:00\n'
        parsed = self.tool.parse_cue(valid, {'track3.bin'})
        self.assertEqual(parsed[0]['indexes'], {'1': 0})
        for data in (valid.replace(b'track3.bin', b'../track3.bin'),
                     valid.replace(b'00:00:00', b'00:60:00'),
                     valid + b'POSTGAP 00:02:00\n',
                     valid + b' INDEX 01 00:00:00\n'):
            with self.assertRaises(ValueError):
                self.tool.parse_cue(data, {'track3.bin'})

    def test_successful_hashes_cannot_bypass_game_specific_validation(self):
        output = self.allowed / 'bad-cue'
        def reject(_):
            raise ValueError('Unsafe synthetic CUE')
        with self.assertRaises(ValueError):
            self.tool.extract_and_verify(self.archive, output, Path('7z.exe'), self.expected,
                                         [self.allowed], runner=self.runner(), free_bytes=10000,
                                         margin=100, validators=[reject])
        self.assertEqual(json.loads((output / 'import-verification.json').read_text())['status'], 'failed')

    def test_ip_metadata_uses_flycast_layout_and_detects_wince(self):
        sector = bytearray(2352)
        sector[:12] = bytes([0] + [255] * 10 + [0])
        sector[15] = 1
        header = bytearray(b' ' * 256)
        for offset, value in ((0, b'SEGA SEGAKATANA '), (16, b'SEGA ENTERPRISES '),
                              (32, b'0C3B GD-ROM1/1'), (56, b'8799A10'),
                              (64, b'T9507N'), (74, b'V1.000'), (96, b'1ST_READ.BIN')):
            header[offset:offset + len(value)] = value
        sector[16:272] = header
        actual = self.tool.parse_ip_sector(sector)
        self.assertEqual(actual['product_number'], 'T9507N')
        self.assertEqual(actual['boot_filename'], '1ST_READ.BIN')
        self.assertFalse(actual['isWindowsCE'])
        sector[16 + 62] = ord('1')
        self.assertTrue(self.tool.parse_ip_sector(sector)['isWindowsCE'])


if __name__ == '__main__':
    unittest.main()
