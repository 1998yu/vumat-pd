# Runnable case inputs

This directory contains JSON inputs, not a second solver or a duplicate demo suite.
Run an input from the checkout root with
`build/release/bin/vumat_pd_run examples/<name>.json`.

| Inputs | Purpose |
|---|---|
| affine_bar_elastic, affine_shear_elastic | Prescribed affine deformation and interface checks |
| jc_material_point | Local constitutive history without free-interior dynamics |
| elastic_tension_pd, jc_tension_pd | Free-interior PD tensile demonstrations |

The [demo guide](../demos/README.md) launches these inputs and renders their outputs.
The [tensile benchmark](../benchmarks/tensile3d/README.md) is a separate, fixed
comparison protocol with an independent linear-hexahedral FEM reference, retained
data and metrics. Keep reusable inputs here and quantitative reference datasets
under benchmarks; do not duplicate the same case in several folders.

See [parameters](../docs/parameters.md) before changing geometry or units.
