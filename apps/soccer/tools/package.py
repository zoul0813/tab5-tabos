#!/usr/bin/env python3
"""Package a freshly built Soccer ELF; normally invoked by make package."""
import argparse
import hashlib
from pathlib import Path
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[3]


def package(binary, output):
    sources = {
        "T/bin/soccer": binary,
        "T/share/licenses/soccer/LICENSE": ROOT / "apps/soccer/LICENSE",
        "docs/soccer.md": ROOT / "docs/soccer.md",
        "docs/soccer-acceptance.md": ROOT / "docs/soccer-acceptance.md",
        "docs/soccer-visual-trial.md": ROOT / "docs/soccer-visual-trial.md",
    }
    if output.resolve() in {path.resolve() for path in sources.values()}:
        raise ValueError("output must not replace a package input")
    files = {name: path.read_bytes() for name, path in sources.items()}
    elf = files["T/bin/soccer"]
    if len(elf) < 52 or elf[:6] != b"\x7fELF\x01\x01" or elf[18:20] != b"\xf3\x00":
        raise ValueError("expected a little-endian RV32 ELF; run make package with the TabOS toolchain")
    files["README.txt"] = (
        "Soccer for TabOS - local pre-release application bundle\n\n"
        "Copy the contents of T/ to the matching folders on your TabOS drive.\n"
        "Keep share/licenses/soccer/LICENSE with the executable.\n"
        "Launch soccer, or soccer --profile for an exit-only timing report.\n"
        "Use the matching TabOS runtime: this bundle is not firmware.\n"
        "Controls and remaining hardware acceptance are described in docs/.\n"
    ).encode()
    files["SHA256SUMS"] = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n"
        for name, data in sorted(files.items())
    ).encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, suffix=".zip", delete=False) as stream:
            temporary = Path(stream.name)
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.create_system = 3
                entry.external_attr = (0o100755 if name == "T/bin/soccer" else 0o100644) << 16
                entry.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(entry, data)
        with zipfile.ZipFile(temporary) as archive:
            if archive.testzip() is not None:
                raise ValueError("archive CRC verification failed")
            for name, data in files.items():
                if archive.read(name) != data:
                    raise ValueError(f"archive content mismatch: {name}")
        temporary.replace(output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    print(f"Verified Soccer bundle: {output}")
    print(f"SHA-256: {hashlib.sha256(output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        package(args.binary, args.output)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"soccer package: {error}\n")
