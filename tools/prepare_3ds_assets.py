#!/usr/bin/env python3
"""Stage a user-supplied, checksum-verified USA ROM for the existing extractor."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

USA_SHA1 = "9bef1128717f958171a4afac3ed78ee2bb4e86ce"

def prepare(rom, root=Path(".")):
    assets = json.loads((root / "assets.json").read_text())
    missing = [name for name, data in assets.items()
               if not name.startswith("@") and "us" in data[-1]
               and not (root / name).is_file()]
    marker = root / ".assets-local.txt"
    revision = 7
    if marker.exists():
        try:
            revision = int(marker.read_text().splitlines()[1])
        except (ValueError, IndexError):
            revision = -1
    if not missing and revision == 7:
        # Existing repository assets need no ROM. The extractor checks its revision.
        if not marker.exists():
            marker.write_text("# Asset extraction revision\n7\n" + "\n".join(
                name for name in assets if not name.startswith("@")) + "\n")
        return
    source = Path(rom)
    if not source.is_file():
        raise ValueError(f"Missing or outdated assets ({len(missing)} absent). Supply ROM=/path/to/USA.z64 "
                         "(or ROM=/mounted-sd/sm64/game.z64). Extraction runs on the host.")
    with source.open("rb") as stream:
        checksum = hashlib.sha1()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    if checksum.hexdigest() != USA_SHA1:
        raise ValueError("ROM must be an unmodified big-endian USA Super Mario 64 .z64")
    destination = root / "baserom.us.z64"
    if source.resolve() != destination.resolve():
        if destination.exists():
            raise ValueError("baserom.us.z64 already exists; move it before selecting another ROM")
        shutil.copyfile(source, destination)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", default="baserom.us.z64")
    args = parser.parse_args()
    try:
        prepare(args.rom)
    except (OSError, ValueError) as error:
        parser.exit(1, f"3DS assets: {error}\n")
