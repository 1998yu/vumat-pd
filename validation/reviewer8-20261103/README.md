# Local verification on 3 November 2026

Base commit 198c800, branch reviewer-interface-audit-20261031, plus the local
changes hashed in tested-source.json. This is verification in the existing
Windows checkout, not a new clean clone or a remote CI run.

- build.log: rebuilt production runner and verification executables with GNU 16.1.
- tests.log: 21/21 CTest cases passed, including JSON input and optional Eigen.
- csv-regression.json: all 14 numerical demo/material CSVs match the prior baseline.
- archive.log: all 41 archive files checked, with the existing JSON newline rule.
- metrics.log and compare.log: original 11-case, matched-pair and symmetry metrics
  recomputed and verified with unchanged tolerances; these are not new solves.
- time-study.json and dt_x*.log: three new coarse PD solves at 50/25/12.5 ns,
  source/plugin/executable/result hashes and matched-time comparisons.
- tested-source.json: source hashes checked against the actual new study.

Reproduce from the checkout root with the GNU compiler/runtime on PATH:

```bash
cmake --preset release -DVUMAT_PD_EIGEN_CHECK=ON -DEIGEN3_INCLUDE_DIR=/path/to/eigen
cmake --build --preset release --parallel 4
ctest --preset release --output-on-failure
python -m pip install -r benchmarks/tensile3d/requirements.txt
python benchmarks/tensile3d/unpack_archive.py
python benchmarks/tensile3d/analyse_and_plot.py
python benchmarks/tensile3d/verify_metrics.py
python benchmarks/tensile3d/time_step_study.py --output benchmarks/tensile3d/generated/time-study-new --threads 2
```

The study directory must be new. Complete raw new results remain in the tested
checkout's ignored benchmarks/tensile3d/generated/reviewer8-20261103-time.
Public logs replace local checkout and compiler roots with placeholders.
The study used the bundled Python 3.12.14 interpreter and the existing GNU build;
plot/metric reproduction used Python 3.11.4 with NumPy 1.26.4 and Matplotlib 3.11.2.
The report is temporal sensitivity evidence for the documented nonfracturing
case, not a proven stability limit or validation of physical thermal coupling.
