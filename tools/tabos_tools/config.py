from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

from .common import LOCAL_CONFIG, LOCAL_DIR, ROOT, fail


CONFIG_DEFAULTS = {
    "TABOS_HOST_ROOTFS": ".local/rootfs",
    "TABOS_FILESYSTEM_MAX_FILES": "32",
    "TABOS_FILESYSTEM_MAX_DIRECTORIES": "8",
    "TABOS_POINTER_MAX_CONTACTS": "5",
    "TABOS_FONT_FILE": "graphics/blueterm.f12",
    "TABOS_FONT_GLYPH_WIDTH": "8",
    "TABOS_FONT_GLYPH_HEIGHT": "12",
    "TABOS_FONT_GLYPH_COUNT": "256",
    "TABOS_FONT_CELL_WIDTH": "8",
    "TABOS_FONT_CELL_HEIGHT": "15",
    "TABOS_TERMINAL_SCALE": "2",
    "TABOS_TERMINAL_SCROLLBACK_LINES": "256",
    "TABOS_HOST_REFRESH_RATE_HZ": "58",
    "TABOS_CURSOR_BLINK_INTERVAL_MS": "500",
    "TABOS_ENABLE_PIE": "ON",
    "TABOS_ENABLE_PIE_DIAGNOSTICS": "OFF",
    "TABOS_ELF_STARTUP_PATH": "T:/bin/hello",
    "TABOS_SHELL_PATH": "T:/bin/shell",
    "TABOS_HOST_STARTUP_APP": "shell",
    "TABOS_TAB5_STARTUP_APP": "shell",
}


def validate_project_config(config: dict[str, str]) -> None:
    unknown = sorted(set(config) - set(CONFIG_DEFAULTS))
    if unknown:
        fail(f"unknown option in {LOCAL_CONFIG}: {unknown[0]}")
    if not re.fullmatch(r"[1-8]", config["TABOS_TERMINAL_SCALE"]):
        fail("TABOS_TERMINAL_SCALE must be an integer from 1 through 8")
    if not config["TABOS_HOST_ROOTFS"]:
        fail("TABOS_HOST_ROOTFS must not be empty")
    if not re.fullmatch(r"(?:[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])",
                        config["TABOS_FILESYSTEM_MAX_FILES"]):
        fail("TABOS_FILESYSTEM_MAX_FILES must be an integer from 1 through 255")
    if not re.fullmatch(r"(?:[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])",
                        config["TABOS_FILESYSTEM_MAX_DIRECTORIES"]):
        fail("TABOS_FILESYSTEM_MAX_DIRECTORIES must be an integer from 1 through 255")
    if not re.fullmatch(r"(?:[1-9]|[12][0-9]|3[0-2])", config["TABOS_POINTER_MAX_CONTACTS"]):
        fail("TABOS_POINTER_MAX_CONTACTS must be an integer from 1 through 32")
    if not config["TABOS_ELF_STARTUP_PATH"]:
        fail("TABOS_ELF_STARTUP_PATH must not be empty")
    if not config["TABOS_SHELL_PATH"]:
        fail("TABOS_SHELL_PATH must not be empty")
    for name in (
        "TABOS_FONT_GLYPH_WIDTH",
        "TABOS_FONT_GLYPH_HEIGHT",
        "TABOS_FONT_CELL_WIDTH",
        "TABOS_FONT_CELL_HEIGHT",
    ):
        if not re.fullmatch(r"[1-9][0-9]*", config[name]):
            fail(f"{name} must be a positive integer")
    for name in ("TABOS_ENABLE_PIE", "TABOS_ENABLE_PIE_DIAGNOSTICS"):
        if config[name] not in {"ON", "OFF"}:
            fail(f"{name} must be ON or OFF")
    if not re.fullmatch(r"(?:[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-6])",
                        config["TABOS_FONT_GLYPH_COUNT"]):
        fail("TABOS_FONT_GLYPH_COUNT must be an integer from 1 through 256")
    glyph_width = int(config["TABOS_FONT_GLYPH_WIDTH"])
    glyph_height = int(config["TABOS_FONT_GLYPH_HEIGHT"])
    glyph_count = int(config["TABOS_FONT_GLYPH_COUNT"])
    if int(config["TABOS_FONT_CELL_WIDTH"]) < glyph_width or \
            int(config["TABOS_FONT_CELL_HEIGHT"]) < glyph_height:
        fail("font cell dimensions must contain the configured glyph dimensions")
    font_path = Path(config["TABOS_FONT_FILE"])
    if not font_path.is_absolute():
        font_path = ROOT / font_path
    if not font_path.is_file():
        fail(f"TABOS_FONT_FILE does not exist: {font_path}")
    expected_size = ((glyph_width + 7) // 8) * glyph_height * glyph_count
    if font_path.stat().st_size != expected_size:
        fail(
            f"TABOS_FONT_FILE is {font_path.stat().st_size} bytes; expected {expected_size} "
            f"for {glyph_count} glyphs of {glyph_width}x{glyph_height}"
        )
    for name in (
        "TABOS_TERMINAL_SCROLLBACK_LINES",
        "TABOS_HOST_REFRESH_RATE_HZ",
        "TABOS_CURSOR_BLINK_INTERVAL_MS",
    ):
        if not re.fullmatch(r"[1-9][0-9]*", config[name]):
            fail(f"{name} must be a positive integer")
    if config["TABOS_HOST_STARTUP_APP"] not in {
        "none", "console-test", "filesystem-test", "elf-hello", "shell"
    }:
        fail("TABOS_HOST_STARTUP_APP must be none, console-test, filesystem-test, elf-hello, or shell")
    if config["TABOS_TAB5_STARTUP_APP"] not in {
        "none", "console-test", "filesystem-test", "elf-hello", "shell"
    }:
        fail("TABOS_TAB5_STARTUP_APP must be none, console-test, filesystem-test, elf-hello, or shell")


def load_project_config() -> dict[str, str]:
    config = CONFIG_DEFAULTS.copy()
    if LOCAL_CONFIG.exists():
        for line_number, raw_line in enumerate(LOCAL_CONFIG.read_text(encoding="utf-8").splitlines(), 1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                fail(f"invalid line {line_number} in {LOCAL_CONFIG}")
            name, value = line.split("=", 1)
            config[name.strip()] = value.strip()
    validate_project_config(config)
    return config


def prompt_choice(label: str, current: str, choices: tuple[str, ...]) -> str:
    while True:
        value = input(f"{label} ({'/'.join(choices)}) [{current}]: ").strip()
        if not value:
            return current
        if value in choices:
            return value
        print(f"tabos: choose one of: {', '.join(choices)}")


def prompt_integer(label: str, current: str, minimum: int, maximum: int | None = None) -> str:
    while True:
        value = input(f"{label} [{current}]: ").strip()
        if not value:
            return current
        if value.isdigit() and int(value) >= minimum and (maximum is None or int(value) <= maximum):
            return value
        limit = f"{minimum} through {maximum}" if maximum is not None else f"{minimum} or greater"
        print(f"tabos: enter integer {limit}")


def prompt_text(label: str, current: str) -> str:
    value = input(f"{label} [{current}]: ").strip()
    return value or current


def command_config(_args: argparse.Namespace) -> None:
    config = load_project_config()
    print(f"TabOS project configuration ({LOCAL_CONFIG.relative_to(ROOT)})")
    print("Press Enter to keep current value. Ctrl+C aborts without saving.\n")
    try:
        config["TABOS_HOST_STARTUP_APP"] = prompt_choice(
            "Host startup app", config["TABOS_HOST_STARTUP_APP"],
            ("none", "console-test", "filesystem-test", "elf-hello", "shell")
        )
        config["TABOS_TAB5_STARTUP_APP"] = prompt_choice(
            "Tab5 startup app", config["TABOS_TAB5_STARTUP_APP"],
            ("none", "console-test", "filesystem-test", "elf-hello", "shell")
        )
        config["TABOS_FILESYSTEM_MAX_FILES"] = prompt_integer(
            "Maximum open files", config["TABOS_FILESYSTEM_MAX_FILES"], 1, 255
        )
        config["TABOS_FILESYSTEM_MAX_DIRECTORIES"] = prompt_integer(
            "Maximum open directories", config["TABOS_FILESYSTEM_MAX_DIRECTORIES"], 1, 255
        )
        config["TABOS_POINTER_MAX_CONTACTS"] = prompt_integer(
            "Maximum pointer contacts", config["TABOS_POINTER_MAX_CONTACTS"], 1, 32
        )
        config["TABOS_ELF_STARTUP_PATH"] = prompt_text(
            "Filesystem-backed ELF startup path", config["TABOS_ELF_STARTUP_PATH"]
        )
        config["TABOS_SHELL_PATH"] = prompt_text(
            "Filesystem-backed shell path", config["TABOS_SHELL_PATH"]
        )
        config["TABOS_HOST_ROOTFS"] = prompt_text(
            "Host root filesystem directory", config["TABOS_HOST_ROOTFS"]
        )
        config["TABOS_FONT_FILE"] = prompt_text("Bitmap font file", config["TABOS_FONT_FILE"])
        config["TABOS_FONT_GLYPH_WIDTH"] = prompt_integer(
            "Font glyph width (pixels)", config["TABOS_FONT_GLYPH_WIDTH"], 1
        )
        config["TABOS_FONT_GLYPH_HEIGHT"] = prompt_integer(
            "Font glyph height (pixels)", config["TABOS_FONT_GLYPH_HEIGHT"], 1
        )
        config["TABOS_FONT_GLYPH_COUNT"] = prompt_integer(
            "Font glyph count", config["TABOS_FONT_GLYPH_COUNT"], 1, 256
        )
        config["TABOS_FONT_CELL_WIDTH"] = prompt_integer(
            "Font cell width (pixels)", config["TABOS_FONT_CELL_WIDTH"], 1
        )
        config["TABOS_FONT_CELL_HEIGHT"] = prompt_integer(
            "Font cell height (pixels)", config["TABOS_FONT_CELL_HEIGHT"], 1
        )
        config["TABOS_TERMINAL_SCALE"] = prompt_integer(
            "Terminal scale", config["TABOS_TERMINAL_SCALE"], 1, 8
        )
        config["TABOS_TERMINAL_SCROLLBACK_LINES"] = prompt_integer(
            "Terminal scrollback lines", config["TABOS_TERMINAL_SCROLLBACK_LINES"], 1
        )
        config["TABOS_HOST_REFRESH_RATE_HZ"] = prompt_integer(
            "Host graphics refresh rate (Hz)", config["TABOS_HOST_REFRESH_RATE_HZ"], 1
        )
        config["TABOS_CURSOR_BLINK_INTERVAL_MS"] = prompt_integer(
            "Cursor blink half-period (ms)", config["TABOS_CURSOR_BLINK_INTERVAL_MS"], 1
        )
        config["TABOS_ENABLE_PIE"] = prompt_choice(
            "Tab5 PIE SIMD acceleration", config["TABOS_ENABLE_PIE"], ("ON", "OFF")
        )
        config["TABOS_ENABLE_PIE_DIAGNOSTICS"] = prompt_choice(
            "Tab5 PIE SIMD boot diagnostics", config["TABOS_ENABLE_PIE_DIAGNOSTICS"],
            ("ON", "OFF")
        )
    except (EOFError, KeyboardInterrupt):
        print("\ntabos: configuration aborted", file=sys.stderr)
        raise SystemExit(2)

    validate_project_config(config)
    LOCAL_DIR.mkdir(parents=True, exist_ok=True)
    temporary = LOCAL_CONFIG.with_suffix(".config.tmp")
    contents = "# Generated by ./tools/tabos config\n" + "".join(
        f"{name}={config[name]}\n" for name in CONFIG_DEFAULTS
    )
    temporary.write_text(contents, encoding="utf-8")
    os.replace(temporary, LOCAL_CONFIG)
    print(f"tabos: saved {LOCAL_CONFIG.relative_to(ROOT)}")


def project_cmake_arguments(target: str) -> list[str]:
    config = load_project_config()
    startup_app = config["TABOS_TAB5_STARTUP_APP" if target == "tab5" else "TABOS_HOST_STARTUP_APP"]
    font_path = Path(config["TABOS_FONT_FILE"])
    if not font_path.is_absolute():
        font_path = ROOT / font_path
    host_rootfs = Path(config["TABOS_HOST_ROOTFS"])
    if not host_rootfs.is_absolute():
        host_rootfs = ROOT / host_rootfs
    return [
        f"-DTABOS_HOST_ROOTFS={host_rootfs}",
        f"-DTABOS_FILESYSTEM_MAX_FILES={config['TABOS_FILESYSTEM_MAX_FILES']}",
        f"-DTABOS_FILESYSTEM_MAX_DIRECTORIES={config['TABOS_FILESYSTEM_MAX_DIRECTORIES']}",
        f"-DTABOS_POINTER_MAX_CONTACTS={config['TABOS_POINTER_MAX_CONTACTS']}",
        f"-DTABOS_FONT_FILE={font_path}",
        f"-DTABOS_FONT_GLYPH_WIDTH={config['TABOS_FONT_GLYPH_WIDTH']}",
        f"-DTABOS_FONT_GLYPH_HEIGHT={config['TABOS_FONT_GLYPH_HEIGHT']}",
        f"-DTABOS_FONT_GLYPH_COUNT={config['TABOS_FONT_GLYPH_COUNT']}",
        f"-DTABOS_FONT_CELL_WIDTH={config['TABOS_FONT_CELL_WIDTH']}",
        f"-DTABOS_FONT_CELL_HEIGHT={config['TABOS_FONT_CELL_HEIGHT']}",
        f"-DTABOS_TERMINAL_SCALE={config['TABOS_TERMINAL_SCALE']}",
        f"-DTABOS_TERMINAL_SCROLLBACK_LINES={config['TABOS_TERMINAL_SCROLLBACK_LINES']}",
        f"-DTABOS_HOST_REFRESH_RATE_HZ={config['TABOS_HOST_REFRESH_RATE_HZ']}",
        f"-DTABOS_CURSOR_BLINK_INTERVAL_MS={config['TABOS_CURSOR_BLINK_INTERVAL_MS']}",
        f"-DTABOS_ENABLE_PIE={config['TABOS_ENABLE_PIE']}",
        f"-DTABOS_ENABLE_PIE_DIAGNOSTICS={config['TABOS_ENABLE_PIE_DIAGNOSTICS']}",
        f"-DTABOS_ELF_STARTUP_PATH={config['TABOS_ELF_STARTUP_PATH']}",
        f"-DTABOS_SHELL_PATH={config['TABOS_SHELL_PATH']}",
        f"-DTABOS_ENABLE_CONSOLE_DIAGNOSTIC_APP={'ON' if startup_app == 'console-test' else 'OFF'}",
        f"-DTABOS_ENABLE_FILESYSTEM_DIAGNOSTIC_APP={'ON' if startup_app == 'filesystem-test' else 'OFF'}",
        f"-DTABOS_ENABLE_ELF_LOADER_EXPERIMENT={'ON' if startup_app == 'elf-hello' else 'OFF'}",
        f"-DTABOS_ENABLE_SHELL_STARTUP={'ON' if startup_app == 'shell' else 'OFF'}",
    ]
