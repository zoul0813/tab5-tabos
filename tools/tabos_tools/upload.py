from __future__ import annotations

import argparse
import shutil

from .common import ROOT, fail, run
from .environment import idf_environment


def command_upload(args: argparse.Namespace) -> None:
    if not args.upload_files:
        fail("upload requires --file SOURCE DESTINATION (relative to the SD-card root)")
    environment = idf_environment()
    python = shutil.which("python", path=environment.get("PATH"))
    if python is None:
        fail("ESP-IDF Python unavailable")
    command = [python, str(ROOT / "tools" / "msc_upload.py")]
    if args.port:
        command += ["--port", args.port]
    for source, destination in args.upload_files:
        command += ["--file", source, destination]
    run(command, env=environment)
