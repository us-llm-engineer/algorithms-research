# Reproducible benchmarks

Release benchmarks are built with `ALGO_BUILD_BENCH=ON`, write CSV rows through `common/bench_harness.hpp`, and
are rendered into module READMEs by `tools/render_tables.py`. Run the complete pipeline with:

```bash
python3 tools/run_benchmarks.py
```

Each row records the algorithm, workload, input size, median/minimum/maximum wall time, repetition count, and an
algorithm-specific note. CSV files are the source of truth for the generated Markdown tables.
