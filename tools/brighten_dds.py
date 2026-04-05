#!/usr/bin/env python3
import argparse
from pathlib import Path
import shutil

from PIL import Image
import numpy as np


def process_file(path: Path, gamma: float):
    img = Image.open(path)
    mode = img.mode
    if mode not in ("RGB", "RGBA"):
        img = img.convert("RGBA")
    arr = np.array(img).astype(np.float32) / 255.0
    arr = np.clip(arr, 0.0, 1.0)
    # apply gamma (gamma < 1 brightens)
    arr = np.power(arr, gamma)
    arr = (arr * 255.0).astype(np.uint8)
    out = Image.fromarray(arr, mode="RGBA")
    # preserve alpha if original didn't have alpha
    if mode == "RGB":
        out = out.convert("RGB")
    out.save(path)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--dir", required=True, help="Directory containing .dds files")
    p.add_argument("--gamma", type=float, default=0.6, help="Gamma value to apply (gamma <1 brightens)")
    p.add_argument("--backup", action="store_true", help="Create backup of originals")
    args = p.parse_args()

    base = Path(args.dir)
    if not base.exists():
        print("Directory not found:", base)
        return
    files = [f for f in base.iterdir() if f.is_file() and f.suffix.lower() == ".dds"]
    if not files:
        print("No .dds files found in", base)
        return

    if args.backup:
        backup_dir = base.parent / (base.name + "_backup")
        backup_dir.mkdir(exist_ok=True)
        for f in files:
            shutil.copy2(f, backup_dir / f.name)
        print("Backed up originals to", backup_dir)

    for f in files:
        try:
            print("Processing", f.name)
            process_file(f, args.gamma)
        except Exception as e:
            print("Failed to process", f.name, e)

    print("Done")


if __name__ == "__main__":
    main()
