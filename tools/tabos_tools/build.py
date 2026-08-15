from __future__ import annotations

import argparse
import platform
import shutil

from .common import ROOT, fail, require_tool, run
from .config import project_cmake_arguments
from .environment import idf_environment


HOST_TARGETS = {"macos", "linux"}
CONFIGURATIONS = {"debug": "Debug", "release": "Release"}


def validate_host(target: str) -> None:
    host = platform.system()
    expected = "Darwin" if target == "macos" else "Linux"
    if host != expected:
        fail(f"target '{target}' requires a {expected} host; current host is {host}")


def host_build(target: str, configuration: str) -> None:
    validate_host(target)
    cmake = require_tool("cmake", "install CMake 3.22 or newer")
    require_tool("ninja", "install Ninja")
    preset = f"{target}-{configuration}"
    run([cmake, "--preset", preset, *project_cmake_arguments(target)])
    run([cmake, "--build", "--preset", preset])


def tab5_command(configuration: str, action: str) -> None:
    environment = idf_environment()
    idf = shutil.which("idf.py", path=environment.get("PATH"))
    if idf is None:
        fail("idf.py missing after ESP-IDF activation; run './tools/tabos setup'")
    build_dir = ROOT / "build" / f"tab5-{configuration}"
    command = [
        idf,
        "-C",
        str(ROOT / "targets" / "tab5"),
        "-B",
        str(build_dir),
        "-DIDF_TARGET=esp32p4",
        f"-DCMAKE_BUILD_TYPE={CONFIGURATIONS[configuration]}",
        *project_cmake_arguments("tab5"),
        action,
    ]
    run(command, env=environment)


def command_build(args: argparse.Namespace) -> None:
    if args.target in HOST_TARGETS:
        host_build(args.target, args.configuration)
    else:
        tab5_command(args.configuration, "build")


def command_test(args: argparse.Namespace) -> None:
    if args.target not in HOST_TARGETS:
        fail("tests currently run only on macos and linux host targets")
    host_build(args.target, args.configuration)
    ctest = require_tool("ctest", "install CMake 3.22 or newer")
    run([ctest, "--preset", f"{args.target}-{args.configuration}"])


def command_run(args: argparse.Namespace) -> None:
    if args.target not in HOST_TARGETS:
        fail("run supports host targets; use 'tab5 flash' for hardware")
    host_build(args.target, args.configuration)
    executable = ROOT / "build" / f"{args.target}-{args.configuration}" / "tabos_host"
    run([str(executable)])


def command_flash(args: argparse.Namespace) -> None:
    tab5_command(args.configuration, "flash")
