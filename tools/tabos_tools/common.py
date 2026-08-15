from __future__ import annotations

import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ESP_IDF_VERSION = "v5.4.4"
LOCAL_DIR = ROOT / ".local"
LOCAL_IDF = LOCAL_DIR / "esp-idf"
LOCAL_IDF_TOOLS = LOCAL_DIR / "espressif-tools"
LOCAL_CONFIG = LOCAL_DIR / "tabos.config"


def fail(message: str) -> None:
    print(f"tabos: {message}", file=sys.stderr)
    raise SystemExit(2)


def identity_value(name: str) -> str:
    identity_file = ROOT / "config" / "Identity.cmake"
    contents = identity_file.read_text(encoding="utf-8")
    match = re.search(rf'^set\({re.escape(name)} "([^"]+)"\)$', contents, re.MULTILINE)
    if match is None:
        fail(f"could not read {name} from {identity_file}")
    return match.group(1)


def require_tool(name: str, hint: str) -> str:
    executable = shutil.which(name)
    if executable is None:
        fail(f"required tool '{name}' not found; {hint}")
    return executable


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def command_succeeds(command: list[str]) -> bool:
    return subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    ).returncode == 0


def confirm_commands(summary: str, commands: list[list[str]]) -> None:
    print(f"tabos: {summary}")
    for command in commands:
        print("  " + shlex.join(command))
    try:
        answer = input("Continue? [y/N] ").strip().lower()
    except EOFError:
        answer = ""
    if answer not in {"y", "yes"}:
        print("tabos: setup aborted")
        raise SystemExit(0)
