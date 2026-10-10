import sys
from pathlib import Path
import tempfile
import unittest
import plistlib
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import msc_upload


class UploadTests(unittest.TestCase):
    def test_macos_volume_identity(self):
        info = {"Internal": False, "BusProtocol": "USB", "VolumeName": "TAB5",
                "WritableVolume": True, "WritableMedia": True}
        with patch.object(Path, "is_mount", return_value=True):
            with patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(stdout=plistlib.dumps(info))):
                self.assertEqual(msc_upload.mounted_card(), Path("/Volumes/TAB5"))
            info["WritableVolume"] = False
            with patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(stdout=plistlib.dumps(info))):
                with self.assertRaises(RuntimeError):
                    msc_upload.mounted_card()

    def test_destinations(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "tester"
            source.write_bytes(b"new")
            for destination in ("../tester", "/bin/tester", "bin/../../tester", "T:/bin/tester", ""):
                with self.assertRaises(ValueError):
                    msc_upload.validate_files([[str(source), destination]])
            with self.assertRaises(ValueError):
                msc_upload.validate_files([[str(source), "bin/tester"], [str(source), "BIN/TESTER"]])

    def test_verified_replacement_and_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.write_bytes(b"new app")
            card = root / "card"
            (card / "bin").mkdir(parents=True)
            target = card / "bin/tester"
            target.write_bytes(b"old app")
            files = msc_upload.validate_files([[str(source), "bin/tester"]])
            msc_upload.install_files(card, files, root / "backup")
            self.assertEqual(target.read_bytes(), b"new app")
            self.assertEqual((root / "backup/bin/tester").read_bytes(), b"old app")
            with patch.object(msc_upload, "digest", side_effect=["expected", "wrong"]):
                with self.assertRaises(RuntimeError):
                    msc_upload.install_files(card, files, root / "backup2")
            self.assertEqual(target.read_bytes(), b"new app")

    def test_symlink_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            card = root / "card"
            card.mkdir()
            (card / "bin").symlink_to(root / "outside", target_is_directory=True)
            source = root / "source"
            source.write_bytes(b"app")
            with self.assertRaises(ValueError):
                msc_upload.install_files(card, [(source, Path("bin/tester"))], root / "backup")
            self.assertFalse((root / "outside").exists())


if __name__ == "__main__":
    unittest.main()
