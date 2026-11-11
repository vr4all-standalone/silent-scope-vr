import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile
import zlib


class MediaTests(unittest.TestCase):
    def setUp(self):
        module_path = Path(__file__).parents[1] / 'tools' / 'media.py'
        self.assertTrue(module_path.exists(), 'Production media audit is not implemented')
        spec = importlib.util.spec_from_file_location('scope_media', module_path)
        self.media = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.media)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.program = b'owned game program'
        self.nvram = b'legacy timekeeper'
        self.archive = self.root / 'sscope.zip'
        self.manifest = {
            'roms': [self.entry('program.bin', self.program), self.entry('clock_ua', b'new timekeeper')],
            'legacyNvram': [self.entry('old_clock', self.nvram)],
            'nvramName': 'clock_ua',
        }

    @staticmethod
    def entry(name, data):
        return {'name': name, 'size': len(data), 'crc': f'{zlib.crc32(data):08x}',
                'sha1': hashlib.sha1(data).hexdigest()}

    def write_zip(self, entries):
        with zipfile.ZipFile(self.archive, 'w') as archive:
            for name, data in entries.items():
                archive.writestr(name, data)

    def test_exact_match_and_crc_filename_alias(self):
        self.write_zip({'alternate-name.bin': self.program, 'clock_ua': b'new timekeeper'})
        result = self.media.audit_archive(self.archive, self.manifest)
        self.assertTrue(result['exact'])
        self.assertEqual(result['missing'], [])
        self.assertEqual(result['matched'][0]['archiveName'], 'alternate-name.bin')

    def test_wrong_contents_under_expected_name_are_rejected(self):
        self.write_zip({'program.bin': b'bad game program', 'clock_ua': b'new timekeeper'})
        result = self.media.audit_archive(self.archive, self.manifest)
        self.assertFalse(result['exact'])
        self.assertEqual(result['mismatched'][0]['name'], 'program.bin')
        self.assertEqual(result['missing'], [])

    def test_legacy_nvram_is_distinct_from_exact_set_and_original_unchanged(self):
        self.write_zip({'program.bin': self.program, 'old_clock': self.nvram})
        original = self.archive.read_bytes()
        result = self.media.audit_archive(self.archive, self.manifest)
        self.assertFalse(result['exact'])
        self.assertEqual(result['missing'], ['clock_ua'])
        self.assertEqual(result['legacyNvram'][0]['archiveName'], 'old_clock')
        override = self.media.prepare_diagnostic_nvram(self.archive, self.root / 'override', 'old_clock', self.manifest)
        self.assertEqual(override.read_bytes(), self.nvram)
        self.assertEqual(override.name, 'clock_ua')
        self.assertEqual(self.archive.read_bytes(), original)
        self.assertTrue((self.root / 'override' / 'DIAGNOSTIC-ONLY.json').exists())

    def test_unknown_nvram_candidate_is_rejected_without_output(self):
        self.write_zip({'old_clock': b'corrupt timekeeper'})
        with self.assertRaises(ValueError):
            self.media.prepare_diagnostic_nvram(self.archive, self.root / 'override', 'old_clock', self.manifest)
        self.assertFalse((self.root / 'override').exists())

    def test_missing_program_and_duplicate_filename_are_rejected(self):
        self.write_zip({'old_clock': self.nvram})
        self.assertIn('program.bin', self.media.audit_archive(self.archive, self.manifest)['missing'])
        with zipfile.ZipFile(self.archive, 'a') as archive:
            archive.writestr('program.bin', self.program)
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter('ignore')
                archive.writestr('program.bin', b'duplicate')
        with self.assertRaises(ValueError):
            self.media.audit_archive(self.archive, self.manifest)


if __name__ == '__main__':
    unittest.main()
