#!/usr/bin/env python3
"""Request a one-shot Tab5 MSC boot and install explicit files on macOS or Linux."""
from __future__ import annotations

import argparse
import hashlib
import json
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
        if any(normalized.startswith(old + "/") or old.startswith(normalized + "/") for old in seen):
            raise ValueError(f"conflicting destination: {destination}")
        if any(part.endswith((" ", ".")) or any(ord(c) < 32 or c in '<>"|?*' for c in part)
               for part in rel.parts):
            raise ValueError(f"invalid FAT destination: {destination}")
        seen.add(normalized)
        result.append((src, Path(str(rel))))
    return result


def request_msc(port: str | None) -> None:
    try:
        import serial
        from serial.tools import list_ports
    except ImportError as error:
        raise RuntimeError("pyserial unavailable; activate ESP-IDF first") from error
    if shutil.which("lsof") is None:
        raise RuntimeError("lsof unavailable; install it before accessing serial")
    if port is None:
        ports = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        if len(ports) != 1:
            raise RuntimeError("expected one Tab5 USB-C serial device; use --port")
        port = ports[0]
    holders = subprocess.run(["lsof", "-t", port], capture_output=True, text=True, timeout=10)
    if holders.returncode not in (0, 1) or holders.stderr.strip():
        raise RuntimeError("could not check serial-port ownership: " + holders.stderr.strip())
    if holders.stdout.strip():
        raise RuntimeError("serial port is in use; stop the monitor before uploading")
    try:
        connection = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2, exclusive=True)
        # Set control lines before opening; opening must not reset an active app.
        connection.dtr = False
        connection.rts = False
        connection.port = port
        with connection:
            request = b"\nTABOS MSC\n"
            if connection.write(request) != len(request):
                raise RuntimeError("partial MSC request; inspect device before retrying")
            # No flush: tcdrain can block indefinitely on disconnected USB serial.
            deadline = time.monotonic() + 20
            received = bytearray()
            while time.monotonic() < deadline:
                received.extend(connection.read(4096))
                if b"TABOS MSC BUSY" in received:
                    raise RuntimeError("Tab5 is busy; exit the foreground app before uploading")
                if b"TABOS MSC OK" in received:
                    print("tabos: MSC request accepted", flush=True)
                    return
                received = received[-8192:]
    except (serial.SerialException, OSError) as error:
        raise RuntimeError(f"serial failure on {port}: {error}; check connection and dialout permissions; "
                           "request may be partially sent; no retry attempted") from error
    raise RuntimeError("MSC request timed out; inspect device before retrying; wait for the shell")


def check_dependencies() -> None:
    names = {"darwin": ("diskutil",), "linux": ("lsblk", "udisksctl", "pkcheck")}
    if sys.platform not in names:
        raise RuntimeError("MSC upload supports macOS and Linux")
    for name in names[sys.platform]:
        if shutil.which(name) is None:
            raise RuntimeError(f"{name} unavailable; install storage tools before copying")


def linux_cards() -> list[tuple[Path | None, str, str]]:
    result = subprocess.run(["lsblk", "--json", "--tree", "--output",
                             "PATH,TYPE,TRAN,LABEL,RO,MOUNTPOINTS"],
                            capture_output=True, text=True, check=True, timeout=10)
    cards = []
    disk_mounts = {}
    def visit(node, usb=False, disk=None):
        usb = usb or node.get("tran") == "usb"
        disk = node["path"] if node.get("type") == "disk" else disk
        if disk:
            disk_mounts.setdefault(disk, []).extend(m for m in node.get("mountpoints", []) or [] if m)
        if node.get("label") == "TAB5":
            if not usb or node.get("ro") or disk is None:
                raise RuntimeError("TAB5 must be a writable USB disk")
            mounts = [m for m in node.get("mountpoints", []) or [] if m]
            for mount in mounts:
                cards.append((Path(mount), node["path"], disk))
            if not mounts:
                cards.append((None, node["path"], disk))
        for child in node.get("children", []):
            visit(child, usb, disk)
    for node in json.loads(result.stdout)["blockdevices"]:
        visit(node)
    for _, _, disk in cards:
        if len(disk_mounts[disk]) > 1:
            raise RuntimeError("TAB5 disk has multiple mounted filesystems; refusing whole-device eject")
    return cards


def mounted_card(override: str | None = None) -> Path | None:
    if sys.platform == "linux":
        cards = [card for card in linux_cards() if card[0] is not None]
        if override:
            cards = [card for card in cards if card[0].resolve() == Path(override).resolve()]
        if len(cards) > 1:
            raise RuntimeError("ambiguous TAB5 volumes; use --mount")
        if not cards:
            return None
        mount = cards[0][0]
        if not mount.is_mount() or not os.access(mount, os.W_OK | os.X_OK):
            raise RuntimeError(f"card mount is unavailable or not writable: {mount}")
        return mount
    if override is None:
        candidates = []
        for volume in Path("/Volumes").iterdir():
            if volume.is_mount():
                info = subprocess.run(["diskutil", "info", "-plist", str(volume)],
                                      capture_output=True, check=True, timeout=10)
                if plistlib.loads(info.stdout).get("VolumeName") == "TAB5":
                    candidates.append(volume)
        if len(candidates) > 1:
            raise RuntimeError("ambiguous TAB5 volumes; use --mount")
        if not candidates:
            return None
        override = str(candidates[0])
    mount = Path(override).resolve()
    if not mount.is_mount():
        return None
    info = subprocess.run(["diskutil", "info", "-plist", str(mount)], capture_output=True, check=True, timeout=10)
    disk = plistlib.loads(info.stdout)
    if (disk.get("Internal", True) or disk.get("BusProtocol") != "USB" or
            disk.get("VolumeName") != "TAB5" or not disk.get("WritableVolume", False) or
            not disk.get("WritableMedia", False)):
        raise RuntimeError(f"{mount} is not a writable external USB volume")
    if not os.access(mount, os.W_OK | os.X_OK):
        raise RuntimeError(f"card mount is not writable: {mount}")
    return mount


def eject_card(mount: Path) -> None:
    # Revalidate identity immediately before touching the device.
    if mounted_card(str(mount)) != mount:
        raise RuntimeError("card mount changed before eject")
    os.sync()
    if sys.platform == "darwin":
        subprocess.run(["diskutil", "eject", str(mount)], check=True, timeout=60)
    else:
        card = next(card for card in linux_cards() if card[0] == mount)
        subprocess.run(["udisksctl", "unmount", "--block-device", card[1], "--no-user-interaction"],
                       check=True, timeout=60)
        # Power-off flushes caches and detaches USB; unmount alone can leave MSC active.
        subprocess.run(["udisksctl", "power-off", "--block-device", card[2], "--no-user-interaction"],
                       check=True, timeout=60)


def preflight_files(mount: Path, files: list[tuple[Path, Path]]) -> None:
    for src, rel in files:
        if not os.access(src, os.R_OK):
            raise RuntimeError(f"source is not readable: {src}")
        target = mount / rel
        if not target.resolve().is_relative_to(mount.resolve()):
            raise ValueError(f"destination escapes card: {rel}")
        parent = target.parent
        while not parent.exists():
            parent = parent.parent
        if not parent.is_dir() or not os.access(parent, os.W_OK | os.X_OK):
            raise RuntimeError(f"destination is not writable: {parent}")
        if target.exists() and not target.is_file():
            raise RuntimeError(f"destination is not a file: {target}")
        if target.with_name(target.name + ".tabos-upload").exists():
            raise RuntimeError(f"unfinished upload exists: {target}")
    if shutil.disk_usage(mount).free < sum(src.stat().st_size for src, _ in files):
        raise RuntimeError("insufficient free space on card")


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(65536), b""):
            sha.update(chunk)
    return sha.hexdigest()


def install_files(mount: Path, files: list[tuple[Path, Path]], backup: Path) -> None:
    preflight_files(mount, files)
    backup.mkdir(parents=True, exist_ok=True)
    if not os.access(backup, os.W_OK | os.X_OK):
        raise RuntimeError(f"backup directory is not writable: {backup}")
    for src, rel in files:
        target = mount / rel
        if not target.resolve().is_relative_to(mount.resolve()):
            raise ValueError(f"destination escapes card: {rel}")
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            saved = backup / rel
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(target, saved)
            if digest(saved) != digest(target):
                raise RuntimeError(f"backup verification failed: {rel}; original retained")
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
    parser.add_argument("--mount", default=os.environ.get("TABOS_MSC_MOUNT"))
    parser.add_argument("--mounted", action="store_true", help="install to an already mounted card")
    parser.add_argument("--file", nargs=2, action="append", required=True)
    args = parser.parse_args()
    try:
        check_dependencies()
        files = validate_files(args.file)
        mount = mounted_card(args.mount)
        if args.mounted:
            if mount is None:
                raise RuntimeError("validated TAB5 mount not found; mount the USB card first")
        else:
            # Even an override typo must never open serial while a card is
            # exported. Some USB serial drivers reset the board on open.
            if (mount is not None or
                    (sys.platform == "linux" and linux_cards()) or
                    (sys.platform == "darwin" and mounted_card() is not None)):
                raise RuntimeError("TAB5 is already exported; safely eject it before requesting an automated upload")
            request_msc(args.port)
            deadline = time.monotonic() + 45
            while mount is None and time.monotonic() < deadline:
                time.sleep(0.25)
                mount = mounted_card(args.mount)
            if mount is None:
                raise RuntimeError("MSC disk did not mount; connect USB-A data cable and mount TAB5 in the file manager")
        if sys.platform == "linux":
            # Active local sessions normally have this Polkit grant. Never prompt
            # for authorization after copying, or invoke sudo from upload tooling.
            permission = subprocess.run(["pkcheck", "--action-id", "org.freedesktop.udisks2.power-off-drive",
                                         "--process", str(os.getpid())],
                                        capture_output=True, text=True, timeout=10)
            if permission.returncode != 0:
                raise RuntimeError("no permission to power off USB disk; use an active local desktop session "
                                   "or ask an administrator to configure UDisks/Polkit")
        backup = ROOT / ".local" / "msc-backups" / str(time.time_ns())
        install_files(mount, files, backup)
        eject_card(mount)
        print("tabos: upload complete; Tab5 restarts after eject", flush=True)
        return 0
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"tabos: {error}", file=sys.stderr)
        print("tabos: no further writes attempted; safely eject any mounted TAB5 disk after inspection", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
