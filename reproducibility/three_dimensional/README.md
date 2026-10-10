# Three-dimensional FEM–PD verification (2026-10-10)

This is the current reproducibility package for Section 3.1 and Figures 4–6.
It replaces the historical comparison of a 2D FEM mesh and a PD slab with
matched **100 × 20 × 4 mm** three-dimensional models. Refinement preserves
the physical thickness. Both paths use the same independently authored,
BSD-3-Clause Johnson–Cook material plugin; the FEM spatial assembly is
independent of PD. The FEM is an in-house H8 solver, not Abaqus.

## Model and reference

- Half-cosine axial displacement: 2 mm over 0.12 ms; mm–ms–N–MPa units.
- Axial support at x ≤ 2 mm and loading at x ≥ 98 mm on every mesh.
- Lateral and thickness faces are free. The support-centre pin fixes y and z;
  an additional support-edge pin fixes z to suppress rigid-body rotation.
- PD control volumes are clipped at the domain boundary: face, edge and
  corner weights are 1/2, 1/4 and 1/8. Total volume is 8000 mm³ and solver
  mass is 62.4, with density 0.0078 N ms²/mm⁴.
- FEM uses full 2 × 2 × 2 Gauss integration, a total-Lagrangian Piola force
  assembly, lumped mass and explicit velocity Verlet integration.
- The common numerical reference has 150 × 30 × 6 H8 elements,
  h = 2/3 mm and Δt = 25 ns. Two reflection symmetry planes reduce the
  reference computation to a quarter domain; nodal and scalar field outputs
  reconstruct the full domain. The exterior thickness faces remain free.
  Full-domain versus quarter-domain coarse results provide a symmetry check.
- Damage, deletion, damping and mass scaling are disabled. The finest FEM
  solution is a numerical approximation, not an exact or experimental truth.

| Case | h (mm) | Horizon (mm) | Δt (ns) | Purpose |
|---|---:|---:|---:|---|
| fem_coarse | 2 | — | 25 | Coarse 3D FEM |
| fem_fine | 1 | — | 25 | Fine 3D FEM |
| fem_reference | 2/3 | — | 25 | Common reference |
| fem_timecheck | 1 | — | 12.5 | FEM time-step check |
| pd_coarse | 2 | 6 | 25 | Figure 4 |
| pd_shared | 1 | 3 | 25 | Figure 5 and shared baseline |
| pd_m1p5 | 2 | 3 | 25 | Fixed-horizon grid study |
| pd_delta2 | 1 | 2 | 25 | Horizon study |
| pd_delta4 | 1 | 4 | 25 | Horizon study |
| pd_dt50 | 1 | 3 | 50 | Time-step study |
| pd_dt12p5 | 1 | 3 | 12.5 | Time-step study |

Figure 6 compares every case with the same finest 3D FEM reference and
includes coarse/fine FEM baselines. Its studies are grid refinement at fixed
horizon, horizon variation at fixed grid, and time-step variation. They do
not use adjacent “Levels 1–2” or “Levels 2–3” differences.

## Recreate the archived figures without solving

Install Python 3.10+ and the plotting dependencies, then run from the repo root:

```bash
python -m pip install -r reproducibility/three_dimensional/requirements.txt
python reproducibility/three_dimensional/analyse_and_plot.py
python reproducibility/three_dimensional/verify_metrics.py
python reproducibility/three_dimensional/plot_geometry.py
python reproducibility/three_dimensional/plot_verification_path.py
```

The analyzer expands `raw/*/*.csv.gz` into the ignored `generated/` directory
and verifies SHA-256 against the original, uncompressed CSV files before
calculating errors. `expected/` holds the frozen metrics and hashes.
`figures/` contains editable SVGs, PDFs and preview PNGs. New figures are
written to `generated/figures/`, including 600 dpi PNG/TIFF exports.
The archived CSV content is losslessly compressed to keep the repository small.

## Recompute the simulations

Use CMake, Ninja, GCC/gfortran and OpenMP. Keep the compiler runtime libraries
on PATH on Windows. Use a checkout path without non-ASCII characters because
the lightweight C++ JSON parser does not decode Unicode path escapes.
Build and run the eight automated checks first:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
python reproducibility/three_dimensional/run_cases.py --threads 3 --workers 4
python reproducibility/three_dimensional/analyse_and_plot.py --results reproducibility/three_dimensional/generated/results
python reproducibility/three_dimensional/verify_metrics.py
```

The case runner also runs `fem_symmetry_check`. It detects Windows `.exe`
names, resolves the build and material paths relative to this checkout, and
records each command and log below `generated/`. The default is one worker
and one OpenMP thread; choose more threads for the included reentrant JC
routine. Full recalculation may take substantial time. Run selected cases:

```bash
python reproducibility/three_dimensional/run_cases.py --cases fem_coarse pd_coarse --threads 3 --workers 2
```

Existing successful cases are reused. `--overwrite` reruns selected generated
cases; the published archive is never overwritten. `--bin-dir` selects another
CMake build directory. Numerical tolerances allow platform/compiler variation.

## Error definitions and results

Mean stress/strain use the fixed interior region x = 2–98 mm and full cross
section with volume weights. Stress-history NRMSE uses 240 common times from
0.0005 to 0.12 ms, normalized by the reference peak mean stress. Displacement
RMSE includes all three components at 1551 fixed coordinates and is normalized
by 2 mm; FEM nodal values are trilinearly interpolated. Maximum plastic-strain
deviation uses the final full-domain maximum and the reference maximum.
Contours show the PD middle layer and the two nearest FEM Gauss-point planes
with the same full-domain colour limits for Figures 4 and 5.

For h = 1 mm and horizon = 3 mm, PD deviations from the finest FEM are
**0.483%** (stress history), **0.540%** (displacement), and **8.113%**
(maximum plastic strain). The local plastic maximum remains more sensitive
than mean stress. Table 3 retains direct FEM–PD comparisons at matching h;
these have a different baseline from Figure 6. `expected/direct_pair_metrics.json`
records that distinction explicitly.

Archived verification includes H8 partition/affine/volume/Piola checks,
nine independent neighbor-lattice tests, and exact agreement of 241 coarse
FEM history outputs between serial, streamed and cached parallel execution.
The coarse full/quarter symmetry stress-history NRMSE is 0.000475%; the fine
FEM time-step history NRMSE is 0.0511%. The analyzer repeats the archived
symmetry/cache comparisons when those audit files are available.

## Scope and historical baseline

This frozen solver revision is specialized for the 100 mm x-directed tensile
verification. Its boundary-case averaging region is x = 2–98 mm; it should
not be interpreted as a general-purpose boundary-value solver. Streamed
material updates use the corotational path. OpenMP material blocks require
reentrant VUMATs; arbitrary user routines need their own thread-safety audit.
The current tensor/shear mapping and thermal-work conventions are inherited
from the research adapter and do not establish universal Abaqus VUMAT
compatibility or a physically calibrated adiabatic temperature response.

Raw PD metadata preserves a legacy plane-strain elastic-slope diagnostic.
It is not the reference for this 3D free-sided model; the analyzer recomputes
the diagnostic against E = 210000 MPa. Original raw files and hashes are
preserved.

The previous 2D comparison is retained in the parent reproducibility directory
as historical evidence. Reproduce that earlier model from commit
`913e42635183c87eb2734666c79c328800ff41c2`, because the current solver changes
boundary quadrature and averaging. Use this directory for the revised results.
