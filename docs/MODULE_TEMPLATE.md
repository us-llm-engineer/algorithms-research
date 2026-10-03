# Module README template

Every module README follows the same eight sections.

1. **Problem Statement** - exact inputs, outputs, constraints, invariants.
2. **Algorithm Overview & Mathematical Model** - intuition, pseudo-code, the invariant, and the proofs (correctness and complexity) in LaTeX.
3. **Complexity Analysis** - best / average / worst time and space table, with the model assumptions.
4. **Quickstart & Usage** - a compilable snippet.
5. **Empirical Benchmarks** - hardware, workloads, baselines, a table rendered from `results/*.csv` by `tools/render_tables.py`, and a short reading of the numbers.
6. **Correctness & Verification** - the test categories (`[given] [boundary] [structural] [adversarial] [stress] [scale] [invariant]`), the oracles used, the mutants killed, and the run command.
7. **Limitations & Trade-Offs** - candid, including where a simpler baseline wins.
8. **References** - papers and books with full citations.
