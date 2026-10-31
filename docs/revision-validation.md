# Local revision validation — 2026-10-31

Platform: Windows; GNU C++/Fortran 16.1.0 (MSYS2 UCRT64); Release build, OpenMP
two threads for tests. Optional Eigen 3.4.0 headers were used only for verification.
Formatting used clang-format 23.1.3 and the checked-in style.

| Check | Evidence |
|---|---|
| Baseline suite | 13 original CTest cases passed before behavioral changes |
| Revised suite | 16 cases: original 13 plus observation-window, correspondence-sampling and Eigen rotation checks |
| Output regression | 14 numerical CSV outputs compared byte-for-byte with baseline; metadata intentionally extended |
| FEM reference audit | Full coarse and symmetry cases actually rerun; full coarse CSVs identical to the prior archive |
| Archived results | All 41 CSV/metadata hashes verified; 11 main-case metrics and both direct-pair metrics recomputed with original tolerances |
| Symmetry regression | Refreshed optional archive reproduces the original expected metrics; no thresholds relaxed |
| Static workflow checks | C++ style check (47 src/include/test files), Python compilation, launcher help and git diff whitespace check |

The actual logs, CSV-regression summary and regenerated metric JSON are packaged in
[validation/20261031](../validation/20261031/). The one-time archive repair is
recorded in benchmarks/tensile3d/expected/archive_repair_20261031.json.

The 11-case publication matrix was **not** fully solved again in this code-organization
task. Its metrics were recomputed from checked archives. The actual new solves were
the test-suite demonstrations and the coarse full/reduced FEM audit.
Windows was tested locally; the updated Linux CI configuration has not been run
on the remote service. These checks do not establish general finite-rotation,
thermal, fracture or stabilized-PD validity.
