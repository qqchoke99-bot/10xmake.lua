#!/usr/bin/env python3
import argparse, json, sys, zipfile
from pathlib import Path

LIBRARY_NAME = "libSoundPhysicsLite.so"
MANIFEST = {
    "name": "SoundPhysicsLite",
    "author": "SPL",
    "description": "Sound/Low/Volume Occlusion, Room Reverb, Echo, SOS 343",
    "version": "0.22.0",
    "entry": LIBRARY_NAME,
    "icon": "icon.png",
}

def write_package(library: Path, icon: Path, output: Path) -> None:
    if not library.is_file():
        raise FileNotFoundError(f"Library not found: {library}")
    if not icon.is_file():
        raise FileNotFoundError(f"Icon not found: {icon}")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()
    manifest_bytes = (json.dumps(MANIFEST, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        archive.writestr("manifest.json", manifest_bytes)
        archive.write(library, LIBRARY_NAME)
        archive.write(icon, "icon.png")
    with zipfile.ZipFile(output, "r") as archive:
        names = set(archive.namelist())
        expected = {"manifest.json", LIBRARY_NAME, "icon.png"}
        if names != expected:
            raise RuntimeError(f"Unexpected package entries: {sorted(names)}")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--icon", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        write_package(args.library.resolve(), args.icon.resolve(), args.output.resolve())
    except Exception as error:
        print(error, file=sys.stderr)
        return 1
    print(args.output.resolve())
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
