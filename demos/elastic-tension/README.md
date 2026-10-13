# Elastic tension

[Project capabilities](../../README.md) · [Demo index](../README.md)

This case shows how the same plugin interface supplies elastic response to a dynamic PD solve. The particle animation exposes the axial stress distribution; the curves show mean stress and prescribed end motion.

![Elastic tension: actual PD particle evolution](media/particles.gif)

[High-quality MP4](media/particles.mp4) · [Final-state preview](media/overview.png) · [Rendering provenance](media/particles.json)

## Setup

| Item | Setting |
|---|---|
| Solver | Correspondence peridynamics |
| Material routine | [Elastic.f](../../materials/examples/Elastic.f) |
| Geometry and loading | 100 × 20 × 4 mm; 51 × 11 × 3 particles; end displacement 0.02 mm |
| Discretization and time integration | 2 mm spacing; 6 mm horizon; 4800 increments of 25 ns; duration 0.12 ms |

The 2 mm axial end bands carry a half-cosine displacement ramp. Lateral and thickness faces are free; rigid-body pins remove transverse drift. Other particles move under PD force balance. Boundary-cell volumes are clipped to the body.

Corotational Elastic coupling. No damping, mass scaling, damage or deletion.

[PD input](../../examples/elastic_tension_pd.json) · [Material configuration](../../configs/elastic.json) · [Interface conventions](../../docs/interface.md)

## Reproduce

After building, run these commands from the repository root:

```bash
python demos/run.py elastic
python demos/render_particles.py --case elastic
```

The renderer also exports an editable SVG to the ignored output folder when needed. Use `--preview-only` on the renderer to save initial, middle and final PNGs without encoding a video. Generated particle media appear in `demos/output/particles/elastic-tension`. The shared [rendering guide](../README.md#recompute-and-render) lists Python/FFmpeg dependencies.

## What the view represents

Every colored point is one actual PD particle at its saved deformed 3D coordinates. A fixed orthographic camera preserves equal physical scale. Marker size improves readability and does not represent particle volume. Stress uses a fixed full-history color scale; there is no surface mesh, smoothing or interpolation between saved states.

The [media provenance](media/particles.json) records the particle count, source CSV hashes, actual time range and verified video hash. The [four-case run manifest](../provenance.json) identifies the executed PD programs and material plugins. FEM results are not used in this animation.
