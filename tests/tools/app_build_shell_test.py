"""Exercise fresh-shell activation without a compiler, ESP-IDF install, or hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AppBuildShellTests(unittest.TestCase):
    def test_fresh_shell_activation_uses_bash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "apps/example").mkdir(parents=True)
            (root / "tools").mkdir()
            (root / "bin").mkdir()
            shutil.copyfile(ROOT / "apps/build.sh", root / "apps/build.sh")
            (root / "apps/build.sh").chmod(0o755)
            (root / "apps/example/Makefile").touch()
            # Like IDF export.sh, activation requires Bash even when parent is dash.
            (root / "tools/tabos").write_text(
                '#!/bin/sh\nprintf \'test -n "$BASH_VERSION" || exit 91; export TABOS_ACTIVATED=yes\\n\'\n')
            (root / "tools/tabos").chmod(0o755)
            (root / "bin/make").write_text('#!/bin/sh\ntest "$TABOS_ACTIVATED" = yes\n')
            (root / "bin/make").chmod(0o755)
            env = {"PATH": str(root / "bin") + ":/usr/bin:/bin", "HOME": directory}
            subprocess.run(["/bin/sh", "-c", '"$1" build', "test", str(root / "apps/build.sh")],
                           env=env, check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
