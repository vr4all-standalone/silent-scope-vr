from pathlib import Path
import unittest
import xml.etree.ElementTree as ET


class ScopedStorageTests(unittest.TestCase):
    def test_manifest_does_not_request_broad_game_file_access(self):
        manifest = Path(__file__).parents[1] / 'android/app/src/main/AndroidManifest.xml'
        root = ET.parse(manifest).getroot()
        permissions = {item.attrib['{http://schemas.android.com/apk/res/android}name'] for item in root.findall('uses-permission')}
        self.assertNotIn('android.permission.MANAGE_EXTERNAL_STORAGE', permissions)
        self.assertNotIn('android.permission.READ_EXTERNAL_STORAGE', permissions)


if __name__ == '__main__':
    unittest.main()
