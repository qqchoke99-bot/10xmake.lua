#!/usr/bin/env python3
import argparse, json, zipfile, re
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--library", required=True)
    ap.add_argument("--icon")
    ap.add_argument("--version-header")
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    version = "1.0.0"
    if args.version_header and Path(args.version_header).exists():
        m = re.search(r'SP_VERSION_STRING\s+"([^"]+)"', Path(args.version_header).read_text())
        if m: version = m.group(1)
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    meta = {
        "name": "SoundPhysics",
        "version": version,
        "entry": "libSoundPhysics.so",
        "description": "Realistic sound physics for Bedrock (occlusion, reverb, echo, SoS 343)",
    }
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("levimod.json", json.dumps(meta, indent=2))
        z.write(args.library, "libSoundPhysics.so")
        if args.icon and Path(args.icon).exists():
            z.write(args.icon, "icon.png")
        cfg = Path(__file__).resolve().parent.parent / "config" / "config.json"
        if cfg.exists():
            z.write(cfg, "config/config.json")
    print("packed", out)

if __name__ == "__main__":
    main()
