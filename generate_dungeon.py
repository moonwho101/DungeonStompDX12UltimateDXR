#!/usr/bin/env python3
"""Entry point wrapper for the randomized level1 dungeon generator."""

import runpy
from pathlib import Path

runpy.run_path(
    str(Path(__file__).resolve().parent / "tools" / "generate_dungeon.py"),
    run_name="__main__",
)
