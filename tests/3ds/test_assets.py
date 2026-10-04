import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("assets", "tools/prepare_3ds_assets.py")
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)

class AssetPreparation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "assets.json").write_text(json.dumps({
            "sample.bin": [4, {"us": [0]}], "jp-only.bin": [4, {"jp": [0]}]}))
    def tearDown(self):
        self.temp.cleanup()
    def test_missing_rom_fails_without_creating_assets(self):
        with self.assertRaisesRegex(ValueError, "Supply ROM"):
            assets.prepare(self.root / "missing.z64", self.root)
        self.assertFalse((self.root / ".assets-local.txt").exists())
    def test_wrong_rom_does_not_replace_user_rom(self):
        existing = self.root / "baserom.us.z64"
        existing.write_bytes(b"preserve me")
        invalid = self.root / "wrong.z64"
        invalid.write_bytes(b"wrong version")
        with self.assertRaisesRegex(ValueError, "unmodified"):
            assets.prepare(invalid, self.root)
        self.assertEqual(existing.read_bytes(), b"preserve me")
    def test_complete_usa_assets_skip_rom(self):
        (self.root / "sample.bin").write_bytes(b"data")
        assets.prepare(self.root / "missing.z64", self.root)
        self.assertEqual((self.root / ".assets-local.txt").read_text().splitlines()[1], "7")
    def test_outdated_assets_require_rom(self):
        (self.root / "sample.bin").write_bytes(b"data")
        (self.root / ".assets-local.txt").write_text("revision\n6\n")
        with self.assertRaisesRegex(ValueError, "Supply ROM"):
            assets.prepare(self.root / "missing.z64", self.root)

if __name__ == "__main__":
    unittest.main()
