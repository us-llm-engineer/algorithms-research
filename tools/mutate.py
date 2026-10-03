#!/usr/bin/env python3
"""Mutation check for the hard test suites.

Each module may ship a `mutants.json` next to its CMakeLists.txt:

    {"target": "fibonacci_heap_test",
     "mutants": [{"id": "no-cascading-cut", "file": "include/fibonacci_heap.hpp",
                  "old": "cascading_cut(p);", "new": "/* removed */",
                  "note": "tree height must stay O(log n)"}]}

For every mutant the tool rewrites the file in place (a `.orig` backup is always
restored), rebuilds only the module's test target, runs it, and expects the suite
to FAIL. A mutant that still passes ("survived") is a missing assertion.

    python3 tools/mutate.py --preset debug-asan            # all modules
    python3 tools/mutate.py --module fibonacci-heap        # one module
"""
import argparse
import json
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="debug-asan")
    ap.add_argument("--module", default=None, help="module directory name")
    args = ap.parse_args()

    specs = sorted(ROOT.glob("modules/*/*/mutants.json"))
    if args.module:
        specs = [s for s in specs if s.parent.name == args.module]
    if not specs:
        print("no mutants.json found")
        return 1

    build = ROOT / "build" / args.preset
    survived, killed, errors = [], 0, []
    for spec_path in specs:
        spec = json.loads(spec_path.read_text())
        mod_dir = spec_path.parent
        target = spec["target"]
        for m in spec["mutants"]:
            f = mod_dir / m["file"]
            original = f.read_text()
            if m["old"] not in original:
                errors.append(f"{mod_dir.name}:{m['id']}: pattern not found in {m['file']}")
                continue
            backup = f.with_suffix(f.suffix + ".orig")
            shutil.copy2(f, backup)
            try:
                count = m.get("count", 1)
                f.write_text(original.replace(m["old"], m["new"], count))
                b = run(["cmake", "--build", str(build), "--target", target])
                if b.returncode != 0:
                    # A mutant that does not compile is not a test of the suite; report it.
                    errors.append(f"{mod_dir.name}:{m['id']}: mutant does not compile")
                    continue
                t = run([str(build / "modules" / mod_dir.parent.name / mod_dir.name / target)]
                        if (build / "modules" / mod_dir.parent.name / mod_dir.name / target).exists()
                        else [str(next(build.rglob(target)))], timeout=600)
                if t.returncode == 0:
                    survived.append(f"{mod_dir.name}:{m['id']}")
                    print(f"SURVIVED {mod_dir.name}:{m['id']}  ({m.get('note', '')})")
                else:
                    killed += 1
                    failing = [l.strip() for l in t.stdout.splitlines() if "FAILED" in l or "failed" in l][:1]
                    print(f"killed   {mod_dir.name}:{m['id']}  {failing[0] if failing else ''}")
            finally:
                shutil.move(backup, f)
    # Rebuild once with the pristine sources so build/ is consistent.
    for spec_path in specs:
        run(["cmake", "--build", str(build), "--target", json.loads(spec_path.read_text())["target"]])
    print(f"\nmutants killed: {killed}, survived: {len(survived)}, errors: {len(errors)}")
    for e in errors:
        print("ERROR", e)
    return 0 if not survived and not errors else 2


if __name__ == "__main__":
    sys.exit(main())
