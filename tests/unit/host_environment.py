#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(sys.argv.pop(1)).resolve()
sys.path.insert(0, str(ROOT / "tools"))

from tabos_tools import environment  # noqa: E402


class HostEnvironmentTest(unittest.TestCase):
    def test_macos_setup_installs_openssl_and_configures_keg_root(self) -> None:
        installed = False

        def command_result(command: list[str], **_kwargs: object) -> subprocess.CompletedProcess[str]:
            nonlocal installed
            if command[-3:] == ["list", "--versions", "openssl@3"]:
                return subprocess.CompletedProcess(command, 0 if installed else 1, "")
            if command[-2:] == ["--prefix", "openssl@3"]:
                return subprocess.CompletedProcess(
                    command,
                    0 if installed else 1,
                    "/opt/homebrew/opt/openssl@3\n" if installed else "",
                )
            raise AssertionError(f"unexpected command: {command}")

        def run(command: list[str], **_kwargs: object) -> None:
            nonlocal installed
            self.assertEqual(command, ["/opt/homebrew/bin/brew", "install", "openssl@3"])
            installed = True

        available = {
            "brew": "/opt/homebrew/bin/brew",
            "cmake": "/usr/bin/cmake",
            "ninja": "/usr/bin/ninja",
            "git": "/usr/bin/git",
            "pkg-config": "/usr/bin/pkg-config",
        }
        with (
            patch.object(environment.platform, "system", return_value="Darwin"),
            patch.object(environment.shutil, "which", side_effect=lambda tool: available.get(tool)),
            patch.object(environment, "cmake_is_supported", return_value=True),
            patch.object(environment, "sdl3_is_available", return_value=True),
            patch.object(environment, "command_succeeds", return_value=False),
            patch.object(environment.subprocess, "run", side_effect=command_result),
            patch.object(environment, "confirm_commands"),
            patch.object(environment, "run", side_effect=run),
        ):
            environment.setup_host_tools()
            self.assertEqual(
                environment.host_openssl_cmake_arguments(),
                ["-DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3"],
            )

    def test_linux_setup_installs_openssl_development_package(self) -> None:
        commands: list[list[str]] = []
        available = {
            "apt-get": "/usr/bin/apt-get",
            "cc": "/usr/bin/cc",
            "c++": "/usr/bin/c++",
            "make": "/usr/bin/make",
            "cmake": "/usr/bin/cmake",
            "ninja": "/usr/bin/ninja",
            "python3": "/usr/bin/python3",
            "git": "/usr/bin/git",
            "pkg-config": "/usr/bin/pkg-config",
        }
        with (
            patch.object(environment.platform, "system", return_value="Linux"),
            patch.object(environment.os, "geteuid", return_value=0),
            patch.object(environment.shutil, "which", side_effect=lambda tool: available.get(tool)),
            patch.object(environment, "cmake_is_supported", return_value=True),
            patch.object(environment, "sdl3_is_available", return_value=True),
            patch.object(environment, "command_succeeds", return_value=False),
            patch.object(environment, "confirm_commands"),
            patch.object(environment, "run", side_effect=lambda command: commands.append(command)),
        ):
            environment.setup_host_tools()

        self.assertEqual(
            commands,
            [
                ["/usr/bin/apt-get", "update"],
                ["/usr/bin/apt-get", "install", "-y", "libssl-dev"],
            ],
        )


if __name__ == "__main__":
    unittest.main()
