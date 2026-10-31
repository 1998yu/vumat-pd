# Code revision against reviewer comments — 2026-10-31

Base: 0f76ef2 (numerical release 63b6f62 plus README/license changes).
This reviewer revision includes local validation evidence; it is not a tagged software release.

| Reviewer concern | Change / evidence | Status |
|---|---|---|
| Unstructured, unreadable code | Six source responsibility folders; LLVM-based formatting; comments explaining frames, quadrature, force density, call sequence and padded blocks | Implemented; not a claim that every legacy design issue is resolved |
| PD equation missing before shape tensor | Governing-equation guide starts from continuum balance, then nodal quadrature, F, constitutive transfer and force | Code documentation implemented; manuscript still needs corresponding edit |
| Benchmark detail and publication | Parameter, timestep and H8/FEM descriptions; existing input/data/metric workflow retained | Reproduction checked locally; external deposition pending |
| Zero-energy compensation missing | Explicitly documented as absent; local alternating-displacement diagnostic added | Limitation exposed; stabilization NOT implemented |
| Modular architecture not visible | src/apps, config, host, pd, material, damage; CMake updated, public includes retained | Implemented |
| Inconsistent captions | No Word edit in this code task | Manuscript task outstanding |
| Figure 6 subfigures | No Figure 6 redesign in this code task | Figure task outstanding |
| Mathematical libraries / Flanagan algorithm | Optional Eigen matrix-exponential oracle; rationale distinguishes rotation exponential from the full stretch/spin update | Cross-check implemented; no speed claim or backend replacement |
| H8 terminology | Eight-node linear hexahedra, trilinear natural-coordinate interpolation, 2x2x2 integration stated | Repository docs updated |
| FEM location | Independent included jc_3d_fem verification executable; shared plugin, separate spatial solver; no Abaqus execution | Documented |
| Stable increment | Actual velocity-Verlet flow, user input and temporal sensitivity protocol described; metadata states no automatic stability bound | No spectral stability estimator implemented |
| Undefined parameters | Units, field meanings, property/state index table, geometry/end-band definitions, explicit observation controls | Documented for current tensile/case workflow |
| PeriLab / impact gap | Removed exclusivity framing; contribution stated as inspectable plugin contract and reproducible shared-material verification | Manuscript comparative evidence remains to be written |
| examples/demo/benchmark overlap and paper assets | Defined inputs vs launch/render guides vs fixed comparisons; paper illustrations removed from homepage workflow | Paper assets retained safely; Zenodo upload/DOI pending approval |

## Behavioral change

Mean-stress/strain sampling no longer hard-codes x=2..98 mm. It follows the loading
axis and defaults to the inner constrained-band planes, or explicit
observation.axialMin/axialMax. Invalid or non-grid-aligned bounds are rejected.
The published 100-mm tensile cases retain their existing window and quadrature.
No force, constitutive, rotation or time-integration formula is intentionally changed.
Metadata now reports the observation interval, stabilization status, timestep policy
and actual outer-damage selection. --help describes the runner contract.

## Still open

- Zero-energy stabilization needs a selected formulation and separate verification.
- The archived JC thermal conversion remains unsuitable for validated physical
  temperature-rise claims; recalibration requires new benchmark runs.
- General finite-rotation transfer and fracture behavior are not established by
  the current checks.
- A major-revision response must not mark these as solved by documentation alone.

See build/revision-audit for local baseline/revised logs and CSV regression evidence.

## Additional reproducibility defect fixed

The optional FEM symmetry archive was stale and excluded from the old checksum
manifest. Its plastic-history discrepancy was 0.2553804%, exceeding the existing
0.05% limit. Fresh full/reduced runs reproduced the expected 0.00757945%.
The full-model CSVs were byte-identical to the original main archive. Only the
stale symmetry data were replaced; all 41 archive CSV/metadata files are now hashed,
and the analysis emits explicit failure diagnostics without changing thresholds.
The original archive remains recoverable in the source checkout/Git history and
in this revision's build/revision-audit backup.
