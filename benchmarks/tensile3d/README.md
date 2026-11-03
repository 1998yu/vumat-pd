# Three-dimensional tensile validation

H8 means an eight-node linear hexahedral finite element (trilinear interpolation
in natural coordinates), with full 2x2x2 Gauss integration in the included FEM
reference. FEM is a separate executable in this repository; no Abaqus solver is
called. It shares the material adapter/plugin, not the PD spatial discretisation.

The [parameter guide](../../docs/parameters.md) and [governing equations](../../docs/kinematics.md#governing-equations-and-spatial-discretisation) define the
quantities needed to reproduce the runs. No zero-energy stabilization is present.

This benchmark checks the same JC material plugin through independent correspondence-PD and fully integrated H8 FEM solvers. Both use a 100 × 20 × 4 mm domain, 2 mm end bands and half-cosine displacement reaching 2 mm in 0.12 ms. Lateral and thickness faces are free except for rigid-body pins.

FEM is an independent validation reference, not the project's runtime or gallery solver. Its source is `src/main_jc_3d_fem.cpp` within this benchmark folder and it is built only with `BUILD_TESTING=ON`.

Damage, deletion, damping and mass scaling are disabled. The configuration checks material transfer, not fracture prediction or general VUMAT compatibility.

## Final configurations

| Case | Spacing mm | PD horizon mm | Increment ns | Role |
|---|---:|---:|---:|---|
| fem_coarse | 2 | — | 25 | Coarse FEM |
| fem_fine | 1 | — | 25 | Fine FEM |
| fem_reference | 2/3 | — | 25 | Common numerical reference |
| fem_timecheck | 1 | — | 12.5 | FEM time-increment check |
| pd_coarse | 2 | 6 | 25 | Coarse matched pair |
| pd_shared | 1 | 3 | 25 | Fine matched pair and PD baseline |
| pd_m1p5 | 2 | 3 | 25 | Fixed-horizon grid study |
| pd_delta2 | 1 | 2 | 25 | Horizon study |
| pd_delta4 | 1 | 4 | 25 | Horizon study |
| pd_dt50 | 1 | 3 | 50 | Time-increment study |
| pd_dt12p5 | 1 | 3 | 12.5 | Time-increment study |

The finest numerical reference uses 150 × 30 × 6 H8 elements. Two reflection planes reduce its computation, then full-domain outputs are reconstructed. `fem_symmetry_check` is retained to check this against the full coarse model. The reference is a numerical approximation.

## Inspect the archive

The archive contains 41 checked CSV and metadata files, including the FEM symmetry case. Run the commands below from the repository root.

```bash
python -m pip install -r benchmarks/tensile3d/requirements.txt
python benchmarks/tensile3d/analyse_and_plot.py
python benchmarks/tensile3d/verify_metrics.py
```

Data expand to ignored `generated/archive_results`. SHA-256 checks use the original uncompressed CSVs. Recomputed metrics and editable plots are saved under `generated`.

## Run selected cases

```bash
python benchmarks/tensile3d/run_cases.py \
  --cases fem_coarse pd_coarse --threads 2 --workers 2
```

The runner resolves paths from the checkout, supports Windows `.exe` names, and defaults to one thread and worker. It reuses completed cases; `--overwrite` recomputes selected generated outputs. `--bin-dir` chooses a nondefault build. Windows paths should be ASCII-only and compiler runtimes must be on `PATH`.

Omit `--cases` to run the final matrix. Fine PD and the finest FEM require more computation than the coarse pair. Optional full-domain FEM midplane snapshots change output only:

```bash
python benchmarks/tensile3d/run_cases.py --cases fem_coarse --snapshots 20 --threads 2
```

These optional FEM fields are verification outputs.

## Recompute all paper cases

Build with the release preset described on the [project homepage](../../README.md). Then run:

```bash
python benchmarks/tensile3d/run_cases.py --threads 2 --workers 1 --overwrite
python benchmarks/tensile3d/analyse_and_plot.py --results benchmarks/tensile3d/generated/results
python benchmarks/tensile3d/verify_metrics.py
```

## Definitions and expected response

- Means are volume-weighted over x = 2–98 mm and the full cross-section.
- Stress-history NRMSE uses 240 shared times from 0.0005 to 0.12 ms, normalized by peak reference mean stress.
- Displacement RMSE uses all three components at 1551 common coordinates, normalized by 2 mm. FEM nodes use trilinear sampling.
- Plastic strain uses the global material-point maximum, which may lie outside the displayed slice.

`expected/direct_pair_metrics.json` compares FEM and PD at matched resolutions. `expected/analysis.json` compares sensitivity cases with the finest FEM reference. Keep these baselines distinct.

The fine direct pair gives 0.55% stress-history NRMSE, 0.96% displacement RMSE/2 mm, 11.50% maximum-plastic-strain difference and 0.19% final mean-stress difference. Relative to the finest FEM, the fine PD baseline gives 0.483%, 0.538% and 8.186% in history, displacement and plastic-strain metrics.

Raw metadata retains an earlier elastic-slope diagnostic for hash integrity; the analyzer uses 3D uniaxial E = 210000 MPa. The checked outputs define validation, rather than that embedded earlier diagnostic.

`cases` contains final PD inputs; `raw` contains compressed final data; `expected` contains the two metric baselines and CSV hashes. Comparison plots, tabulated metrics and displacement-sample coordinates are regenerated under `generated`, so duplicate exported figures and tables are not stored in the checkout. The shared PD implementation is in the project root; the independent FEM reference is in this benchmark's `src` directory.

## FEM verification movie

[Supplementary Movie S1](media/fem-tension.mp4) shows the coarse H8 FEM response. It shows axial stress and plastic strain over 0–0.12 ms. Its [provenance](media/fem-tension.json) records its updated MP4 checksum, quadrature-plane sampling and corrected-archive comparison.

The archive uses the corrected Rodrigues update described in the [kinematics guide](../../docs/kinematics.md). The earlier publication snapshot is available at commit `a47fb8af211f61f3aa5c5f7977234d30a4a28f5f`.

## Time increments and sensitivity

The regional PD host uses velocity-Verlet: displacement prediction, prescribed
boundary motion, constitutive/force update, velocity correction, and reapplication
of constraints. `time_integration.dt` is supplied by the user in milliseconds;
the code does not compute or guarantee a critical stable increment.

For a linear conservative semidiscrete system with positive mass and stiffness,
the central-difference bound is dt <= 2/omega_max. The present nonlinear VUMAT host
does not assemble that spectrum. A continuum estimate h/c_p, with
c_p=sqrt(E(1-nu)/(rho(1+nu)(1-2nu))), can screen an input but is not a proven PD
stability bound: neighborhood quadrature, constraints and material tangent matter.
Use solver density in N ms^2/mm^4 and E in MPa in that estimate.

The archived tensile study uses 50, 25 and 12.5 ns PD runs at fixed h=1 mm and
delta=3 mm. The baseline is 25 ns = 0.000025 ms. The benchmark also includes
an FEM 12.5 ns check. These assess sensitivity of monitored histories and fields,
not unconditional stability, an asymptotic convergence order, or suppression of
zero-energy modes.

To repeat the PD temporal sequence:

```bash
python benchmarks/tensile3d/run_cases.py --cases pd_dt50 pd_shared pd_dt12p5 --threads 2
python benchmarks/tensile3d/analyse_and_plot.py --results benchmarks/tensile3d/generated/results
```

The analyzer needs the other reference cases as well; run the full matrix before
the second command if generated/results is incomplete. Do not combine old and new
results without recording the binary revision and configuration. For a new material,
geometry or loading duration, repeat temporal refinement and check finite response,
energy/work histories and the outputs of interest. Passing a CTest case means its
specified acceptance criteria passed, not that convergence has been established.

## New fixed-space time-step runs

For an isolated temporal study without mixing archived and newly computed cases:

```bash
python benchmarks/tensile3d/time_step_study.py --output benchmarks/tensile3d/generated/time-study-01 --threads 2
```

The default is the existing coarse 51 x 11 x 3 node case, h=2 mm and delta=6 mm.
It runs 50, 25 and 12.5 ns to the same 0.12 ms final time. A different single-material
regional PD input can be supplied with `--case`; the same factors of its base dt
are used. The script holds geometry, material, constraints, loading duration and
all physical parameters fixed. It changes increment counts and output intervals
to compare the same physical times; field snapshots are final-state only.

The output directory must be new. Each run keeps its input, execution log and
solver outputs. `study.json` records material, source, executable and result
hashes, exit codes, physical-time history comparisons, final displacement error
and plastic-strain peak values/locations. Relative differences use the finer PD
run in each adjacent pair, not the FEM reference. The script checks completed,
finite output and the solver's declared acceptance tests. It does not turn those
checks into a claim of asymptotic convergence or absence of unstable modes.

The reported h/c_p estimate uses the configured small-strain elastic E, nu and
solver density; it is a screening ratio, not an automatically selected stable dt.
Different constitutive tangents, neighborhood settings and damage can change the
actual limit. This study retains the selected material's thermal convention;
it does not repair or validate the archived Johnson-Cook heat conversion.
