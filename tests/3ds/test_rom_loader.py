"""Exercise actual ROM inspection with synthetic SD files and error routing."""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class RomLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.build.name) / "rom-test"
        subprocess.run([
            "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Isrc",
            "tests/3ds/test_rom.c", "src/platform/3ds/asset_loader.c", "-o", str(cls.exe)
        ], cwd=ROOT, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.sd = tempfile.TemporaryDirectory()
        self.path = Path(self.sd.name) / "sdmc:/sm64/baserom.us.z64"
        self.path.parent.mkdir(parents=True)
        self.header = bytearray(64)
        struct.pack_into(">I", self.header, 0, 0x80371240)
        struct.pack_into(">II", self.header, 0x10, 0x635A2BFF, 0x8B022326)
        self.header[0x20:0x34] = b"SUPER MARIO 64      "
        self.header[0x3B:0x3F] = b"NSME"

    def tearDown(self):
        self.sd.cleanup()

    def inspect(self):
        return subprocess.run([str(self.exe), "inspect"], cwd=self.sd.name,
                              capture_output=True, text=True)

    def write_rom(self, size=0x800000):
        with self.path.open("wb") as stream:
            stream.write(self.header)
            stream.truncate(size)

    def test_offsets(self):
        self.path.write_bytes(self.header)
        subprocess.run([str(self.exe), str(self.path)], check=True, capture_output=True)

    def test_missing_required_rom(self):
        result = self.inspect()
        self.assertEqual(result.returncode, 3)
        self.assertIn("sdmc:/sm64/baserom.us.z64", result.stderr)
        self.assertIn("errno", result.stderr)

    def test_valid_rom(self):
        self.write_rom()
        result = self.inspect()
        self.assertEqual(result.returncode, 0)
        self.assertIn("8388608 bytes; header read: 64", result.stdout)
        self.assertIn("USA revision 0", result.stdout)

    def test_truncated_header(self):
        self.path.write_bytes(b"short")
        result = self.inspect()
        self.assertEqual(result.returncode, 2)
        self.assertIn("header truncated", result.stderr)
        self.assertIn("header read: 5", result.stderr)

    def test_wrong_size(self):
        self.write_rom(0x800001)
        result = self.inspect()
        self.assertEqual(result.returncode, 2)
        self.assertIn("size must be exactly", result.stderr)

    def test_swapped_byte_order(self):
        self.header[:4] = b"\x37\x80\x40\x12"
        self.write_rom()
        result = self.inspect()
        self.assertEqual(result.returncode, 2)
        self.assertIn("byte order at 0x00", result.stderr)

    def test_io_error(self):
        self.path.mkdir()
        result = self.inspect()
        self.assertEqual(result.returncode, 3)
        self.assertIn("errno", result.stderr)

if __name__ == "__main__":
    unittest.main()
