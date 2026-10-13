# Three-dimensional tensile validation

This benchmark checks the same JC material plugin through independent correspondence-PD and fully integrated H8 FEM solvers. Both use a 100 × 20 × 4 mm domain, 2 mm end bands and half-cosine displacement reaching 2 mm in 0.12 ms. Lateral and thickness faces are free except for rigid-body pins.

FEM is an independent validation reference, not the project's runtime or gallery solver. Its source is `src/main_jc_3d_fem.cpp` within this benchmark folder and it is built only with `BUILD_TESTING=ON`. Every public demo runs PD; see [the PD demo guide](../../demos/README.md).

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

```bash
python -m pip install -r benchmarks/tensile3d/requirements.txt
python benchmarks/tensile3d/analyse_and_plot.py
python benchmarks/tensile3d/verify_metrics.py
```

Data expand to ignored `generated/archive_results`. SHA-256 checks use the original uncompressed CSVs. Recomputed metrics and editable plots are saved under `generated`. For the separate coarse-grid PD particle demo, see [Johnson–Cook tension](../../demos/johnson-cook-tension/README.md). Its display is distinct from this benchmark's fine-resolution comparison.

## Run selected cases

```bash
python benchmarks/tensile3d/run_cases.py \
  --cases fem_coarse pd_coarse --threads 2 --workers 2
```

The runner resolves paths from the checkout, supports Windows `.exe` names, and defaults to one thread and worker. It reuses completed cases; `--overwrite` recomputes selected generated outputs. `--bin-dir` chooses a nondefault build. Windows paths should be ASCII-only and compiler runtimes must be on `PATH`.

Omit `--cases` to run the final matrix. Fine PD and the finest FEM cost more than the quick demos. Optional full-domain FEM midplane snapshots change output only:

```bash
python benchmarks/tensile3d/run_cases.py --cases fem_coarse --snapshots 20 --threads 2
```

These optional FEM fields are verification outputs. The particle-gallery rendering workflow uses only PD results; see [the demo guide](../../demos/README.md).

## Definitions and expected response

- Means are volume-weighted over x = 2–98 mm and the full cross-section.
- Stress-history NRMSE uses 240 shared times from 0.0005 to 0.12 ms, normalized by peak reference mean stress.
- Displacement RMSE uses all three components at 1551 common coordinates, normalized by 2 mm. FEM nodes use trilinear sampling.
- Plastic strain uses the global material-point maximum, which may lie outside the displayed slice.

`expected/direct_pair_metrics.json` compares FEM and PD at matched resolutions. `expected/analysis.json` compares sensitivity cases with the finest FEM reference. Keep these baselines distinct.

The fine direct pair gives 0.55% stress-history NRMSE, 0.96% displacement RMSE/2 mm, 11.52% maximum-plastic-strain difference and 0.17% final mean-stress difference. Relative to the finest FEM, the fine PD baseline gives 0.483%, 0.540% and 8.113% in history, displacement and plastic-strain metrics.

Raw metadata retains an earlier elastic-slope diagnostic for hash integrity; the analyzer uses 3D uniaxial E = 210000 MPa. The checked outputs define validation, rather than that embedded earlier diagnostic.

`cases` contains final PD inputs; `raw` contains compressed final data; `expected` contains the two metric baselines and CSV hashes. Comparison plots, tabulated metrics and displacement-sample coordinates are regenerated under `generated`, so duplicate exported figures and tables are not stored in the checkout. The shared PD implementation is in the project root; the independent FEM reference is in this benchmark's `src` directory.

## FEM verification movie

The homepage includes the original [3D FEM tensile movie](media/fem-tension.mp4), also shown as a [GIF preview](media/fem-tension.gif). It is Supplementary Movie S1 for the coarse H8 case, showing actual axial-stress and plastic-strain evolution over 0–0.12 ms. Its [provenance](media/fem-tension.json) records the unchanged MP4 checksum, quadrature-plane sampling and archive comparison. This movie is an independent verification reference; every runnable demo command still launches PD.
