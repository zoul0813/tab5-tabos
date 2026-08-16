from __future__ import annotations

import argparse
import os
import platform
import re
import shlex
import shutil
import subprocess
from pathlib import Path

from .common import (
    ESP_IDF_VERSION,
    LOCAL_DIR,
    LOCAL_IDF,
    LOCAL_IDF_TOOLS,
    ROOT,
    command_succeeds,
    confirm_commands,
    fail,
    require_tool,
    run,
)


def cmake_is_supported() -> bool:
    cmake = shutil.which("cmake")
    if cmake is None:
        return False
    result = subprocess.run([cmake, "--version"], capture_output=True, text=True, check=False)
    match = re.search(r"cmake version (\d+)\.(\d+)", result.stdout)
    return match is not None and tuple(map(int, match.groups())) >= (3, 22)


def sdl3_is_available() -> bool:
    pkg_config = shutil.which("pkg-config")
    return pkg_config is not None and command_succeeds([pkg_config, "--exists", "sdl3"])


def local_idf_installed() -> bool:
    return (LOCAL_IDF / "export.sh").is_file() and (LOCAL_IDF / "install.sh").is_file()


def active_idf_is_supported() -> bool:
    idf_path = os.environ.get("IDF_PATH")
    idf = shutil.which("idf.py")
    if not idf_path or idf is None:
        return False
    try:
        result = subprocess.run(
            [idf, "--version"],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return False
    match = re.search(r"\bESP-IDF\s+v?(\d+\.\d+\.\d+)\b", result.stdout)
    required_version = ESP_IDF_VERSION.removeprefix("v")
    return result.returncode == 0 and match is not None and match.group(1) == required_version


def local_idf_environment() -> dict[str, str]:
    if not local_idf_installed():
        fail("local ESP-IDF is not installed; run './tools/tabos setup'")
    environment = os.environ.copy()
    environment["IDF_TOOLS_PATH"] = str(LOCAL_IDF_TOOLS)
    command = [
        "bash",
        "-c",
        'source "$1" >/dev/null && env -0',
        "tabos-idf-environment",
        str(LOCAL_IDF / "export.sh"),
    ]
    result = subprocess.run(command, cwd=ROOT, env=environment, check=True, capture_output=True)
    return {
        key.decode(): value.decode()
        for entry in result.stdout.split(b"\0")
        if entry
        for key, value in [entry.split(b"=", 1)]
    }


def idf_environment() -> dict[str, str]:
    if active_idf_is_supported():
        return os.environ.copy()
    print(f"tabos: activating local ESP-IDF {ESP_IDF_VERSION}", flush=True)
    return local_idf_environment()


def setup_host_tools() -> None:
    host = platform.system()
    if host == "Darwin":
        brew = require_tool("brew", "install Homebrew from https://brew.sh")
        packages = []
        if not cmake_is_supported():
            packages.append("cmake")
        if shutil.which("ninja") is None:
            packages.append("ninja")
        if not sdl3_is_available():
            packages.append("sdl3")
        if shutil.which("git") is None:
            packages.append("git")
        if packages:
            commands = [[brew, "install", *packages]]
            confirm_commands("missing macOS host tools will be installed:", commands)
            for command in commands:
                run(command)
        else:
            print("tabos: macOS host tools already installed")
        return
    if host == "Linux":
        apt_get = require_tool("apt-get", "use a Debian/Ubuntu host or install prerequisites manually")
        packages = []
        if any(shutil.which(tool) is None for tool in ("cc", "c++", "make")):
            packages.append("build-essential")
        if not cmake_is_supported():
            packages.append("cmake")
        if shutil.which("ninja") is None:
            packages.append("ninja-build")
        if not sdl3_is_available():
            packages.append("libsdl3-dev")
        if shutil.which("python3") is None:
            packages.append("python3")
        if shutil.which("git") is None:
            packages.append("git")
        if not packages:
            print("tabos: Linux host tools already installed")
            return
        prefix = [] if os.geteuid() == 0 else [require_tool("sudo", "run setup as root or install sudo")]
        commands = [
            [*prefix, apt_get, "update"],
            [*prefix, apt_get, "install", "-y", *packages],
        ]
        confirm_commands("missing Linux host tools will be installed:", commands)
        for command in commands:
            run(command)
        return
    fail(f"setup does not support host operating system '{host}'")


def command_setup(_args: argparse.Namespace) -> None:
    setup_host_tools()
    git = require_tool("git", "install Git")
    LOCAL_DIR.mkdir(parents=True, exist_ok=True)
    git_dir = LOCAL_IDF / ".git"
    if not LOCAL_IDF.exists():
        idf_commands = [[
            git, "clone", "--branch", ESP_IDF_VERSION, "--single-branch", "--depth", "1",
            "--recursive", "--shallow-submodules",
            "https://github.com/espressif/esp-idf.git", str(LOCAL_IDF),
        ]]
    elif git_dir.is_dir():
        idf_commands = [
            [git, "-C", str(LOCAL_IDF), "fetch", "--depth", "1", "origin", f"refs/tags/{ESP_IDF_VERSION}"],
            [git, "-C", str(LOCAL_IDF), "checkout", "--detach", "FETCH_HEAD"],
            [git, "-C", str(LOCAL_IDF), "submodule", "update", "--init", "--recursive", "--depth", "1"],
        ]
    else:
        fail(f"{LOCAL_IDF} exists but is not an ESP-IDF Git checkout")

    environment = os.environ.copy()
    environment["IDF_TOOLS_PATH"] = str(LOCAL_IDF_TOOLS)
    install_command = [str(LOCAL_IDF / "install.sh"), "esp32p4"]
    confirm_commands(
        f"ESP-IDF {ESP_IDF_VERSION} and ESP32-P4 tools will be downloaded or updated under .local:",
        [*idf_commands, install_command],
    )
    for command in idf_commands:
        run(command)
    run(install_command, env=environment)
    print("tabos: local ESP-IDF ready")
    print("tabos: activate current shell with: eval \"$(./tools/tabos activate-idf)\"")


def command_activate_idf(_args: argparse.Namespace) -> None:
    if not local_idf_installed():
        fail("local ESP-IDF is not installed; run './tools/tabos setup'")
    print(f"export IDF_TOOLS_PATH={shlex.quote(str(LOCAL_IDF_TOOLS))}")
    print(f". {shlex.quote(str(LOCAL_IDF / 'export.sh'))}")
