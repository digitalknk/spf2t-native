#!/usr/bin/env python3
"""Run the two primary processes in order using one shared output directory."""
import argparse
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--decomp-project", type=Path, required=True)
parser.add_argument("--rom-dir", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--build-dir", type=Path, required=True)
args = parser.parse_args()
for command in (
    [sys.executable, str(args.decomp_project / "tools/decompile.py"), "--rom-dir", str(args.rom_dir), "--output", str(args.output)],
    [sys.executable, str(root / "tools/build.py"), "--decomp", str(args.output / "decomp"), "--build-dir", str(args.build_dir), "--output", str(args.output / "spf2t-native")],
):
    result = subprocess.run(command)
    if result.returncode:
        sys.exit(result.returncode)
