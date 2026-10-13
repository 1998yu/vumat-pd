# PD particle demonstrations

Every demonstration runs correspondence PD with a compiled Fortran material plugin. Each folder contains its own English introduction and media; the project homepage describes the reusable interface and its verified capabilities.

| Demo folder | Material | PD case |
|---|---|---|
| [elastic-tension](elastic-tension/README.md) | Elastic | Dynamic tension with 0.02 mm end displacement |
| [johnson-cook-tension](johnson-cook-tension/README.md) | Johnson–Cook | Elastoplastic tension with 2 mm end displacement |
| [neohookean-bending](neohookean-bending/README.md) | Neo-Hookean | 150° bending |
| [neohookean-torsion](neohookean-torsion/README.md) | Neo-Hookean | 540° torsion |

## Recompute and render

From the repository root, after the [build steps](../README.md#build-and-start):

```bash
python demos/run.py all --threads 2
python -m pip install -r demos/requirements.txt
python demos/render_particles.py --case all
```

Install FFmpeg (`ffmpeg` and `ffprobe` on `PATH`) for MP4/GIF encoding. Use one thread until a custom routine is known to be reentrant. `--bin-dir` on the launcher selects another CMake build. Simulation commands always execute fresh PD cases.

| Command | Output |
|---|---|
| `python demos/run.py elastic` | `demos/output/elastic_pd` |
| `python demos/run.py tensile` | `demos/output/jc_pd` |
| `python demos/run.py hyperelastic` | `demos/output/hyperelastic/bending` and `torsion` |
| `python demos/render_particles.py --case all` | `demos/output/particles/<demo-folder>` |
| `python demos/render_particles.py --case jc --preview-only` | Initial, middle and final particle previews without encoding |

`transient` remains a shortcut that reruns the JC PD solve and generates its particle movie. None of these commands invokes FEM.

## Particle-view contract

- Every colored point is a saved PD particle at its deformed 3D coordinates. All 1683 tensile particles or 1025 Neo-Hookean particles are submitted to the renderer; naturally occluded particles remain in the point cloud.
- The camera uses a fixed orthographic projection and equal physical scale. Marker diameter is for legibility and does not represent particle volume.
- Colors use the exact particle stress and a fixed full-history range for each case. No surface faces, field smoothing or temporal interpolation are used.
- The faint gray reference cloud in the Neo-Hookean cases provides the undeformed shape. The colored cloud uses true displacement scale.
- Curves show saved solver history; the moving dot marks the current saved frame. Initial and final frames are held briefly for readability.

Rendering uses only existing computed data; it does not rerun the solver or alter any field. Tensile saved frames span 0.0005–0.12 ms; Neo-Hookean frames span 0–1.2 ms. Captions and provenance record these actual ranges.

## Media and traceability

Each checked demo folder contains `media/particles.mp4`, `particles.gif`, `overview.png` and `particles.json`. The JSON records the solver, plugin, particle count, source CSV hashes, color range, projection and movie hash. The [PD run manifest](provenance.json) records the executables and plugins used for the four underlying runs.

Generated CSVs and working previews stay under ignored `demos/output`; they are not duplicate public source files. The checked media are published separately in each demo folder. Editable SVG previews can be regenerated with the renderer; they are not stored as duplicate gallery files.

Material-point checks and the independent H8 FEM comparison belong to [verification](../benchmarks/tensile3d/README.md). The Neo-Hookean spatial demos have no independent FEM/experimental validation.
