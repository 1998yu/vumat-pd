# Historical 2D package

**For the current matched 3D FEM–PD comparison and Figures 4–6, use [three_dimensional/](three_dimensional/README.md).** This older package is archived evidence. To rerun it with its original quadrature and averaging, check out commit 913e42635183c87eb2734666c79c328800ff41c2.

# Paper reproducibility package

This directory reproduces the 100 x 20 mm Johnson-Cook FEM-PD comparison in
the VUMAT-PD manuscript. Both solvers load the same compiled VUMAT plugin.
The package verifies constitutive transfer and field response; damage,
element deletion and experimental calibration are outside its scope.

## Archived release contents

- `cases/`: coarse and fine correspondence-PD input decks.
- `src/main_jc_2d_fem.cpp`: independent explicit Q4 FEM solver used for the
  cross-solver comparison.
- `configs/johnson_cook.json`: the ordered property array and VUMAT dimensions.
- `../materials/examples/JohnsonCook.f90`: complete BSD-3-Clause Johnson-Cook
  VUMAT source used by both paths. This independently authored example
  replaces a restricted third-party development file and may be redistributed.
- `raw/`: archived PD and FEM CSV outputs. Every `timeseries.csv` contains a
  header plus 241 common output times; field files support displacement RMSE
  and contour plots.
- `scripts/compare_jc_2d_crosssolver.py`: regenerates Table 3 as JSON and CSV.
- `scripts/plot_jc_2d_crosssolver_contours.py`: regenerates the coarse and fine
  stress/plastic-strain panels used for Figs. 4-5.
- `scripts/verify_reproduction.py`: compares regenerated metrics with
  `expected/` using a 0.5% relative numerical tolerance.
- `reproduce.ps1` and `reproduce.sh`: end-to-end build, test, solve,
  post-process and verification entry points.
- `SHA256SUMS`: hashes of the VUMAT, inputs, scripts and archived outputs.

Generated files are written to `reproducibility/generated/` and ignored by Git
so the archived release evidence is not overwritten.

## Model definition

The strip is 100 x 20 x 1 mm with a half-cosine end displacement of 2 mm over
0.12 ms. The PD grids are 51 x 11 x 3 (spacing 2 mm, horizon 6 mm) and
101 x 21 x 3 (spacing 1 mm, horizon 3 mm). The FEM meshes are 50 x 10 and
100 x 20 four-node plane-strain quadrilaterals with 2 x 2 Gauss integration,
lumped mass and explicit central differences. Two end layers/columns are
excluded from the interior averages. External damage is disabled.

The solver uses mm-ms-N-MPa units and mass density 0.0078 N ms2/mm4. The
ordered VUMAT property array is:

| Index | Property | Value |
|---:|---|---:|
| 1 | Young's modulus, E (MPa) | 210000 |
| 2 | Poisson's ratio, nu | 0.30 |
| 3 | Johnson-Cook A (MPa) | 350 |
| 4 | Johnson-Cook B (MPa) | 500 |
| 5 | rate coefficient, C | 0.025 |
| 6 | hardening exponent, n | 0.36 |
| 7 | thermal exponent, m | 1.0 |
| 8 | reference rate (ms-1) | 1.0 |
| 9 | reference temperature (K) | 293 |
| 10 | melting temperature (K) | 1793 |
| 11 | source density (kg/m3) | 7800 |
| 12 | Taylor-Quinney coefficient | 0.9 |
| 13 | specific heat (J kg-1 K-1) | 500 |
| 14 | work-to-heat factor | 1.0 |

The six state variables are: initialization flag, equivalent plastic strain,
temperature, current yield stress, last plastic-strain increment and cumulative
return-mapping iterations. Equivalent plastic strain is therefore SDV2.

## Run

Requirements are CMake 3.20+, Ninja, a C++17 compiler, a Fortran compiler and
Python 3.10+. The post-processing scripts use only the Python standard library.
From the repository root run either:

```powershell
./reproducibility/reproduce.ps1
```

or:

```bash
./reproducibility/reproduce.sh
```

The scripts first run the six automated unit/smoke tests, then the four field
solutions. On a typical desktop the fine cases take several minutes.

## Expected Table 3 metrics

| Resolution | Metric | PD | FEM | Difference |
|---|---|---:|---:|---:|
| Coarse | Final mean axial stress (MPa) | 406.94 | 397.61 | 2.29% |
| Coarse | Stress-history NRMSE | - | - | 0.99% |
| Coarse | Final mean axial strain | 0.02145 | 0.02155 | 0.50% |
| Coarse | Maximum equivalent plastic strain | 0.03252 | 0.03147 | 3.22% |
| Coarse | Displacement RMSE / 2 mm | - | - | 1.64% |
| Fine | Final mean axial stress (MPa) | 402.64 | 400.84 | 0.45% |
| Fine | Stress-history NRMSE | - | - | 0.55% |
| Fine | Final mean axial strain | 0.02077 | 0.02074 | 0.13% |
| Fine | Maximum equivalent plastic strain | 0.02946 | 0.02776 | 5.76% |
| Fine | Displacement RMSE / 2 mm | - | - | 1.17% |

History NRMSE and displacement RMSE are array-to-array measures, so separate
PD and FEM scalar entries do not exist for those rows. Exact machine-readable
values are in `expected/jc_2d_crosssolver_comparison.json` and `table3.csv`.
