#!/usr/bin/env python3
"""Request a one-shot Tab5 MSC boot and install explicit files on macOS."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import plistlib
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def validate_files(pairs: list[list[str]]) -> list[tuple[Path, Path]]:
    result = []
    seen = set()
    for source, destination in pairs:
        src = Path(source).resolve(strict=True)
        rel = PurePosixPath(destination)
        if (not src.is_file() or rel.is_absolute() or not rel.parts or
                ".." in rel.parts or ":" in destination or "\\" in destination):
            raise ValueError(f"invalid upload: {source} -> {destination}")
        normalized = str(rel).casefold()
        if normalized in seen:
            raise ValueError(f"duplicate destination: {destination}")
        seen.add(normalized)
        result.append((src, Path(str(rel))))
    return result


def request_msc(port: str | None) -> None:
    import serial
    from serial.tools import list_ports
    if port is None:
        ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        if len(ports) != 1:
            raise RuntimeError("expected one Tab5 USB-C serial device; use --port")
        port = ports[0]
    holders = subprocess.run(["lsof", "-t", port], capture_output=True, text=True)
    if holders.returncode not in (0, 1):
        raise RuntimeError("could not check serial-port ownership")
    if holders.stdout.strip():
        raise RuntimeError("serial port is in use; stop the monitor before uploading")
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2, exclusive=True)
    # Set control lines before opening; opening must not reset an active app.
    connection.dtr = False
    connection.rts = False
    connection.port = port
    with connection:
        deadline = time.monotonic() + 20
        next_request = 0.0
        received = bytearray()
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_request:
                connection.write(b"\nTABOS MSC\n")
                connection.flush()
                next_request = now + 1
            received.extend(connection.read(4096))
            if b"TABOS MSC BUSY" in received:
                raise RuntimeError("Tab5 is busy; exit the foreground app before uploading")
            if b"TABOS MSC OK" in received:
                print("tabos: MSC request accepted", flush=True)
                return
            received = received[-8192:]
    raise RuntimeError("MSC request timed out; install firmware with serial MSC control and wait for the shell")


def mounted_card() -> Path | None:
    mount = Path("/Volumes/TAB5")
    if not mount.is_mount():
        return None
    info = subprocess.run(["diskutil", "info", "-plist", str(mount)], capture_output=True, check=True)
    disk = plistlib.loads(info.stdout)
    if (disk.get("Internal", True) or disk.get("BusProtocol") != "USB" or
            disk.get("VolumeName") != "TAB5" or not disk.get("WritableVolume", False) or
            not disk.get("WritableMedia", False)):
        raise RuntimeError("/Volumes/TAB5 is not a writable external USB volume")
    return mount


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(65536), b""):
            sha.update(chunk)
    return sha.hexdigest()


def install_files(mount: Path, files: list[tuple[Path, Path]], backup: Path) -> None:
    for src, rel in files:
        target = mount / rel
        if not target.resolve().is_relative_to(mount.resolve()):
            raise ValueError(f"destination escapes card: {rel}")
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            saved = backup / rel
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(target, saved)
        expected = digest(src)
        temporary = target.with_name(target.name + ".tabos-upload")
        if temporary.exists():
            raise RuntimeError(f"unfinished upload exists: {temporary}")
        with src.open("rb") as source, temporary.open("xb") as output:
            shutil.copyfileobj(source, output)
            output.flush()
            os.fsync(output.fileno())
        if digest(temporary) != expected:
            raise RuntimeError(f"verification failed: {rel}; original retained")
        temporary.replace(target)
        if digest(target) != expected:
            raise RuntimeError(f"installed file verification failed: {rel}")
        print(f"tabos: verified {rel} ({src.stat().st_size} bytes)", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--file", nargs=2, action="append", required=True)
    args = parser.parse_args()
    try:
        if sys.platform != "darwin":
            raise RuntimeError("MSC upload currently supports macOS hosts")
        files = validate_files(args.file)
        if mounted_card() is not None:
            raise RuntimeError("TAB5 is already mounted; eject it before requesting an automated upload")
        request_msc(args.port)
        deadline = time.monotonic() + 45
        mount = mounted_card()
        while mount is None and time.monotonic() < deadline:
            time.sleep(0.25)
            mount = mounted_card()
        if mount is None:
            raise RuntimeError("MSC disk did not mount; connect Tab5 USB-A to the Mac with a data cable")
        backup = ROOT / ".local" / "msc-backups" / str(time.time_ns())
        install_files(mount, files, backup)
        os.sync()
        subprocess.run(["diskutil", "eject", str(mount)], check=True, timeout=60)
        print("tabos: upload complete; Tab5 restarts after eject", flush=True)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"tabos: {error}", file=sys.stderr)
        print("tabos: no further writes attempted; safely eject any mounted TAB5 disk after inspection", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
