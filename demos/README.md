# Run the demos

Build first and run from the repository root. The launcher uses `build/release/bin`, handles Windows `.exe` names, and accepts `--bin-dir` for another build. The default is one OpenMP thread; choose more only for a reentrant material routine.

## 1 Elastic extension and shear

```bash
python demos/run.py elastic
python demos/plot.py --demo elastic
```

Two affine PD checks use the elastic Fortran plugin. Extension constrains transverse strain, so its analytical reference is the constrained elastic modulus, not the free-sided tensile modulus. Shear uses the tensor-shear convention. The checks report finite values, stress errors and global force balance.

Outputs: `examples/output/affine_bar_elastic` and `affine_shear_elastic`; plot: `demos/output/elastic.png`.

## 2 Johnson–Cook material point

```bash
python demos/run.py material-point
python demos/plot.py --demo material-point
```

This calls the material plugin directly, rather than solving a spatial PD model. The prescribed mixed condition has zero lateral y stress and constrained z strain. Outputs are under `examples/output/jc_material_point`, including stress and material-state histories. The temperature state is numerical history, not a physically calibrated temperature-rise prediction.

## 3 Matched 3D FEM and PD tension

```bash
python demos/run.py tensile --threads 2
python demos/plot.py --demo tensile --results generated
```

The default uses the coarse 100 × 20 × 4 mm model: full-domain H8 FEM and PD at 2 mm spacing, the same JC plugin, 2 mm end bands and a half-cosine displacement. FEM is implemented here, without Abaqus.

Use `--resolution fine` for the 1 mm pair, which costs more than the coarse case:

```bash
python demos/run.py tensile --resolution fine --threads 2
python demos/plot.py --demo tensile --results generated --resolution fine
```

Completed cases are reused; `--overwrite` recomputes them. Outputs are under `benchmarks/tensile3d/generated/results`.

Display the shipped, verified fine archive without solving:

```bash
python demos/plot.py --demo tensile
```

The plot labels its data source and resolution. FEM fields average the two Gauss planes nearest z = 0 and use quadrature subcells; PD uses the z = 0 layer. Reference coordinates and shared full-range scales are used, without nodal extrapolation or smoothing.

## 4 Transient FEM animation

Install plotting dependencies and [FFmpeg](https://ffmpeg.org/download.html). Both `ffmpeg` and `ffprobe` must be on `PATH`.

```bash
python demos/run.py transient --threads 2
```

The launcher saves FEM fields every 20 steps and creates `demos/output/transient.mp4` and `transient.gif`. A completed case is rerun if its snapshot file is missing. Use `--overwrite` to regenerate existing movie data.

The video shows axial stress, equivalent plastic strain, prescribed displacement and an axial stress profile compared with its reflection about x = 50 mm. Physical time is 0–0.12 ms; playback is slowed and on-screen time is simulation time. Scales remain fixed. Damage and deletion are disabled, with no fabricated intermediate fields or smoothing.

Render an existing snapshot file:

```bash
python demos/render_transient.py \
  --snapshots benchmarks/tensile3d/generated/results/fem_coarse/movie_slice.csv \
  --output demos/output/transient.mp4 --gif
```

## 5 Hyperelastic large deformation

```bash
python demos/run.py hyperelastic
python demos/render_hyperelastic.py
```

The material check verifies the Neo-Hookean plugin against closed-form spatial Cauchy stress in tension, compression, xy/xz/yz shear, rigid rotation and rotated finite stretch. Then the launcher runs both the 150° bending and 540° torsion examples, with no Abaqus dependency.

Each example uses a 41 × 5 × 5 particle grid spanning 80 × 8 × 8 mm, 2 mm spacing, 6 mm horizon and clipped boundary-cell volumes from the current PD host. Three end layers are fixed and three loaded layers follow a smooth rigid-motion ramp. The remaining particles are advanced by explicit PD force balance. There are 2400 steps of 0.0005 ms, with real field snapshots every 24 steps. Solver density is 0.0078 N·ms²/mm⁴; E = 2000 MPa and nu = 0.30 set the initial Neo-Hookean tangent. Constitutive viscosities are zero; host dynamic relaxation is 8/ms. Damage and deletion are disabled.

Outputs are under `demos/output/hyperelastic/bending` and `torsion`. The renderer creates separate `demos/output/bending.mp4` and `torsion.mp4`, each with a GIF preview, checkpoint PNGs and a provenance JSON. The README displays them one above the other, with a full-size frame for each example. Surface facets connect actual boundary particles, and each face color averages its corner stresses. Deformation is not exaggerated. Plotting dependencies and FFmpeg are required for rendering. The gallery is available under `docs/media`.

Render one example, or inspect its initial, middle and final frames before encoding:

```bash
python demos/render_hyperelastic.py --scenario bending
python demos/render_hyperelastic.py --preview-only
```

The material routine consumes the spatial deformation gradient and returns spatial Cauchy stress. The dedicated demo calls the adapter directly and applies no additional corotational stress rotation. Do not use that routine unchanged through the corotational JC host. Large rotations are demonstrated in this specific path; spatial convergence, damping sensitivity and comparison with independent FEM/experiments remain unverified.

These examples reproduce the earlier bending/torsion setup using the current host's boundary volumes. The displayed outputs are freshly computed; earlier uniform-volume output tables are not mixed into this project.

## Plotting and outputs

```bash
python -m pip install -r demos/requirements.txt
```

Plots also save editable SVGs next to PNGs. The final gallery in `docs/media` can be viewed without local installation. Generated runs and logs are ignored; completion markers are removed before reruns. The raw validation archive is unchanged and checked by hashes when expanded. See [benchmark details](../benchmarks/tensile3d/README.md).
