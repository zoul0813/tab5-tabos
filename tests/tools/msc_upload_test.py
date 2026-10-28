import sys
from pathlib import Path
import tempfile
import unittest
import plistlib
import json
import subprocess
import types
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import msc_upload


class UploadTests(unittest.TestCase):
    def test_macos_volume_identity(self):
        info = {"Internal": False, "BusProtocol": "USB", "VolumeName": "TAB5",
                "WritableVolume": True, "WritableMedia": True}
        with patch.object(msc_upload.sys, "platform", "darwin"), patch.object(msc_upload.os, "access", return_value=True), patch.object(Path, "is_mount", return_value=True):
            with patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(stdout=plistlib.dumps(info))):
                self.assertEqual(msc_upload.mounted_card("/Volumes/TAB5"), Path("/Volumes/TAB5"))
            info["WritableVolume"] = False
            with patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(stdout=plistlib.dumps(info))):
                with self.assertRaises(RuntimeError):
                    msc_upload.mounted_card("/Volumes/TAB5")

    def test_linux_identity_ambiguity_and_override(self):
        def disk(name, transport="usb", readonly=False):
            return {"path": "/dev/" + name, "type": "disk", "tran": transport,
                    "children": [{"path": "/dev/" + name + "1", "type": "part",
                                  "label": "TAB5", "ro": readonly,
                                  "mountpoints": ["/media/" + name]}]}
        def discover(devices, override=None):
            with patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(
                    stdout=json.dumps({"blockdevices": devices}))):
                return msc_upload.mounted_card(override)
        with (patch.object(msc_upload.sys, "platform", "linux"),
              patch.object(Path, "is_mount", return_value=True),
              patch.object(msc_upload.os, "access", return_value=True)):
            self.assertEqual(discover([disk("sdb")]), Path("/media/sdb"))
            for device in (disk("sdb", "sata"), disk("sdb", readonly=True)):
                with self.assertRaises(RuntimeError):
                    discover([device])
            with self.assertRaises(RuntimeError):
                discover([disk("sdb"), disk("sdc")])
            self.assertEqual(discover([disk("sdb"), disk("sdc")], "/media/sdc"), Path("/media/sdc"))
            self.assertIsNone(discover([disk("sdb")], "/tmp/not-a-card"))

    def test_exported_unmounted_card_blocks_serial_request(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source"
            source.write_bytes(b"test")
            with (patch.object(msc_upload.sys, "platform", "linux"),
                  patch.object(msc_upload.sys, "argv", ["msc_upload", "--mount", "/typo",
                                                        "--file", str(source), "bin/test"]),
                  patch.object(msc_upload, "check_dependencies"),
                  patch.object(msc_upload, "mounted_card", return_value=None),
                  patch.object(msc_upload, "linux_cards", return_value=[(None, "/dev/sdb1", "/dev/sdb")]),
                  patch.object(msc_upload, "request_msc") as request):
                self.assertEqual(msc_upload.main(), 2)
                request.assert_not_called()

    def test_eject_failure_is_not_success(self):
        with (patch.object(msc_upload.sys, "platform", "linux"),
              patch.object(msc_upload, "mounted_card", return_value=Path("/media/TAB5")),
              patch.object(msc_upload, "linux_cards", return_value=[(Path("/media/TAB5"), "/dev/sdb1", "/dev/sdb")]),
              patch.object(msc_upload.os, "sync"),
              patch.object(msc_upload.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "unmount")) as run):
            with self.assertRaises(subprocess.CalledProcessError):
                msc_upload.eject_card(Path("/media/TAB5"))
            self.assertEqual(run.call_count, 1)

    def test_serial_failures_never_retry(self):
        class SerialError(Exception):
            pass
        serial = types.ModuleType("serial")
        serial.SerialException = SerialError
        serial.tools = types.ModuleType("serial.tools")
        serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
        modules = {"serial": serial, "serial.tools": serial.tools,
                   "serial.tools.list_ports": serial.tools.list_ports}
        for error in (SerialError("write timeout"), PermissionError("permission denied"),
                      SerialError("disconnected"), None):
            connection = unittest.mock.MagicMock()
            connection.__enter__.return_value = connection
            connection.write.side_effect = error
            connection.write.return_value = 2
            serial.Serial = unittest.mock.Mock(return_value=connection)
            with (patch.dict(sys.modules, modules),
                  patch.object(msc_upload.shutil, "which", return_value="/usr/bin/lsof"),
                  patch.object(msc_upload.subprocess, "run", return_value=SimpleNamespace(
                      returncode=1, stdout="", stderr=""))):
                with self.assertRaises(RuntimeError):
                    msc_upload.request_msc("/dev/ttyACM0")
                self.assertEqual(connection.write.call_count, 1)
        with patch.dict(sys.modules, modules), patch.object(msc_upload.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "lsof unavailable"):
                msc_upload.request_msc("/dev/ttyACM0")

    def test_preflight_checks_all_destinations_before_copying(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            card = root / "card"
            card.mkdir()
            source = root / "source"
            source.write_bytes(b"new")
            (card / "blocked").mkdir()
            with self.assertRaises(RuntimeError):
                msc_upload.install_files(card, [(source, Path("first")), (source, Path("blocked"))], root / "backup")
            self.assertFalse((card / "first").exists())

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
            with patch.object(msc_upload, "digest", side_effect=["backup", "backup", "expected", "wrong"]):
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
