# Run the PD demos

Every gallery simulation uses correspondence PD and a compiled Fortran material plugin. No demo command launches the FEM solver. Build first, then run from the repository root.

```bash
python demos/run.py all --threads 2
```

This recomputes all four spatial cases and writes `demos/output/run_manifest.json`. The manifest records the executed PD program, selected plugin and checksums of the actual results. `--bin-dir` selects another CMake build; the default is `build/release/bin`. Use one thread until a custom material routine is known to be reentrant.

## Cases and materials

| Gallery case | PD executable | Material plugin | Input/output |
|---|---|---|---|
| Elastic tension | `vumat_pd_run` | `elastic_vumat_plugin` | `examples/elastic_tension_pd.json` → `demos/output/elastic_pd` |
| Elastoplastic tension | `vumat_pd_run` | `jc_vumat_plugin` | `examples/jc_tension_pd.json` → `demos/output/jc_pd` |
| 150° bending | `vumat_pd_large_motion_3d` | `neohookean_kv_vumat_plugin` | CLI bending parameters → `demos/output/hyperelastic/bending` |
| 540° torsion | `vumat_pd_large_motion_3d` | `neohookean_kv_vumat_plugin` | CLI torsion parameters → `demos/output/hyperelastic/torsion` |

```bash
python demos/run.py elastic
python demos/run.py tensile
python demos/run.py hyperelastic
```

`transient` is a shortcut that reruns the JC PD case and renders its movie. It uses the same PD result as `tensile`.

## Elastic and Johnson–Cook PD tension

Both cases use a 100 × 20 × 4 mm bar, 51 × 11 × 3 particles, 2 mm spacing, 6 mm horizon and clipped boundary-cell volumes. Three-dimensional correspondence kinematics and corotational material updates feed the PD force state. The 2 mm end bands carry a half-cosine displacement ramp; other particles move under force balance, with free lateral/thickness faces and rigid-body pins.

Each case takes 4800 increments of 25 ns over 0.12 ms. End displacement is 0.02 mm for Elastic and 2 mm for JC. Solver density is 0.0078 N·ms²/mm⁴. No damping, mass scaling, damage or deletion is applied in these tensile cases.

Snapshots are saved every 20 increments. The first movie state is the actual saved state at 0.0005 ms, and the last is 0.12 ms; no fictitious initial or intermediate fields are inserted. Stress fields use the actual z = 0 PD particles in reference coordinates. The elastic movie's second field is displacement; the JC movie's second field is equivalent plastic strain.

```bash
python demos/plot.py --demo elastic
python demos/plot.py --demo tensile
python demos/render_pd.py --case elastic
python demos/render_pd.py --case jc
```

## Neo-Hookean PD bending and torsion

These cases use an 80 × 8 × 8 mm bar, 41 × 5 × 5 particles, 2 mm spacing, 6 mm horizon and the current host's clipped boundary volumes. Three base layers are fixed and three loaded layers follow a smooth rigid-motion ramp. Free interior particles are advanced by explicit PD forces.

There are 2400 increments of 0.0005 ms and snapshots every 24 increments. E = 2000 MPa and nu = 0.30 specify the initial Neo-Hookean tangent. Constitutive viscosities are zero; host dynamic relaxation is 8/ms. No damage/deletion is enabled.

The material consumes the spatial deformation gradient and returns spatial Cauchy stress through `VumatMaterialAdapter`, directly into the PD force assembly. It receives no additional corotational stress rotation. This differs from the corotational Elastic/JC route; do not interchange those material/frame contracts unchanged.

```bash
python demos/render_hyperelastic.py
python demos/render_hyperelastic.py --scenario bending
python demos/render_hyperelastic.py --preview-only
```

The separate bending and torsion movies show true deformation scale. Surface facets connect actual boundary particles, and face colors average their corner stresses. The material's seven finite-stretch/shear/rotation checks pass; spatial convergence and independent FEM/experimental comparison remain unverified.

## Rendering and provenance

```bash
python -m pip install -r demos/requirements.txt
```

Install FFmpeg for MP4/GIF encoding. `render_pd.py` rejects result metadata from a solver other than correspondence PD. Renderers read saved solver states, keep fixed color scales and record data hashes. Files appear under `demos/output`: `elastic`, `transient`, `bending` and `torsion`, each with MP4/GIF/JSON; field plots also save PNG/SVG files. The checked gallery is under `docs/media`.

## Verification tools

The material-point runner, affine checks and H8 FEM comparison are separate verification tools. They are exercised by `ctest --preset release`. For independent JC comparison and final sensitivity metrics, see [the tensile benchmark](../benchmarks/tensile3d/README.md). Its FEM data are not used to render the PD gallery.
