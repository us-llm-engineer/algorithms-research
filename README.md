# algorithms-research

[![CI](https://github.com/us-llm-engineer/algorithms-research/actions/workflows/ci.yml/badge.svg)](https://github.com/us-llm-engineer/algorithms-research/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

Graduate-level algorithm and data-structure implementations in C++20, each paired with a rigorous complexity
and correctness write-up, a hard test suite (Catch2) and reproducible benchmarks against standard baselines.

Each module lives in `modules/<domain>/<name>/` and documents itself in eight sections: problem statement,
mathematical model and proofs, complexity table, quickstart, benchmarks, verification, trade-offs, references
(see [`docs/MODULE_TEMPLATE.md`](docs/MODULE_TEMPLATE.md)).

## Build and test

Requirements: a C++20 compiler (GCC 11+), CMake >= 3.25, Ninja and [vcpkg](https://vcpkg.io) (Catch2 is the only dependency and is installed in manifest mode from `vcpkg.json`).

```bash
export VCPKG_ROOT=$HOME/vcpkg                    # your vcpkg checkout
cmake --preset debug-asan                        # Debug + AddressSanitizer + UBSan
cmake --build --preset debug-asan
ctest --preset debug-asan                        # all tests run under the sanitizers
cmake --preset release && cmake --build --preset release   # -O3 plus benchmark executables
```

Randomized tests are seeded; a failure prints its seed. Re-run with `ALGO_TEST_SEED=<n>` to explore other inputs.

## Layout

```
common/      seeded RNG, benchmark harness, test helpers (header-only)
modules/     one directory per algorithm: include/, tests/, bench/, results/, README.md
tools/       mutation checker (mutate.py), CSV -> README tables (render_tables.py), plots (plot.py)
```

Benchmarks write CSV files to each module's `results/`; the tables in the READMEs are generated from those files.
