#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run_make(application: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["make", "-C", str(application), *arguments],
        check=False,
        capture_output=True,
        text=True,
    )


def filenames(path: Path) -> set[str]:
    return {item.name for item in path.iterdir() if item.is_file()}


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="tabos application assets ") as directory:
        application = Path(directory) / "application with spaces"
        generated = application / "generated"
        generated.mkdir(parents=True)
        (generated / "game.tsp").write_bytes(b"TSP1")
        (generated / "level.tmap").write_bytes(b"TMP1")
        (generated / "tiles.png").write_bytes(b"source png")
        (generated / "level.tmj").write_text("{}", encoding="utf-8")
        (generated / "manifest.json").write_text("{}", encoding="utf-8")
        makefile = f"""\
APP_NAME := fixture
PROJECT_ROOT := .
BUILD_DIR := build/apps/fixture
OUTPUT := $(BUILD_DIR)/fixture
INSTALL_PATH := rootfs/T/bin/fixture
INSTALL_DATA_PATH := rootfs/T/data/fixture
TABOS_RUNTIME_ASSETS ?= generated/game.tsp generated/level.tmap
TABOS_CUSTOM_BUILD := 1
include {ROOT / "sdk/make/application.mk"}
"""
        (application / "Makefile").write_text(makefile, encoding="utf-8")

        result = run_make(application, "stage-assets")
        staged = application / "build/apps/fixture/data"
        if result.returncode != 0 or filenames(staged) != {"game.tsp", "level.tmap"}:
            return 1

        result = run_make(application, "--no-print-directory", "-s", "tabos-list-outputs")
        expected_output = str((application / "build/apps/fixture/fixture").resolve())
        if result.returncode != 0 or result.stdout.strip() != expected_output:
            return 1
        result = run_make(application, "--no-print-directory", "-s", "tabos-list-runtime-assets")
        expected_assets = {
            str((staged / "game.tsp").resolve()),
            str((staged / "level.tmap").resolve()),
        }
        if result.returncode != 0 or set(result.stdout.splitlines()) != expected_assets:
            return 1

        (staged / "obsolete.tsp").write_bytes(b"stale")
        result = run_make(application, "stage-assets", "TABOS_RUNTIME_ASSETS=")
        if result.returncode != 0 or filenames(staged):
            return 1

        installed = application / "rootfs/T/data/fixture"
        installed.mkdir(parents=True)
        (installed / "save.dat").write_bytes(b"user data")
        result = run_make(application, "install-assets")
        if (result.returncode != 0 or filenames(installed) != {"game.tsp", "level.tmap", "save.dat"} or
                (installed / "save.dat").read_bytes() != b"user data"):
            return 1

        result = run_make(application, "stage-assets", "TABOS_RUNTIME_ASSETS=generated/missing.tsp")
        if result.returncode == 0:
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
