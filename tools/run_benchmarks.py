#!/usr/bin/env python3
"""Run every release benchmark and regenerate its README tables."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "release"

def main() -> int:
    subprocess.run(["cmake", "--preset", "release"], cwd=ROOT, check=True)
    subprocess.run(["cmake", "--build", "--preset", "release"], cwd=ROOT, check=True)
    benches = sorted(BUILD.glob("mod_*/**/*_bench"))
    for bench in benches:
        module = next((p for p in (ROOT / "modules").glob("*/*") if (BUILD / ("mod_" + p.name) / bench.name).exists()), None)
        if module is None:
            continue
        result = module / "results" / "bench.csv"
        subprocess.run([str(bench), str(result)], cwd=ROOT, check=True)
    subprocess.run([sys.executable, str(ROOT / "tools" / "render_tables.py")], cwd=ROOT, check=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
