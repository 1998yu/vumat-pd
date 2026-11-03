# Reviewer follow-up candidate (base 7538485)

The committed interface revision 198c800 was pushed on reviewer-interface-audit-20261031.
Its [Ubuntu CI run](https://github.com/1998yu/vumat-pd/actions/runs/41162730760)
completed successfully, including archive metrics; main remains at 7538485.
The additional 2026-11-03 work is local and uncommitted; see
[Reviewer 8 follow-up](reviewer8-20261103.md). The previous main run passed compilation/tests but failed metadata hash
verification because its manifest used Windows CRLF bytes. The candidate preserves
exact CSV verification and normalizes only JSON CRLF to LF; metadata values remain checked.

Completed evidence: [fresh-clone validation](interface-audit-validation.md), including
20/20 tests, 14 byte-identical CSVs, archive reproduction and separate thermal runs.

## Changes by category

| Category | Change | Compatibility |
|---|---|---|
| Reproducibility | LF attributes, JSON hash policy, line-ending/corruption tests, full archive metrics in CI | CSVs, numerical thresholds and historical response remain unchanged |
| Interface behavior | Reject legacy_small_strain and spatial plugins in incompatible generic hosts | Previously silently accepted combinations now fail with explanation |
| Interface behavior | Validate optional block array sizes before batching; reject nonfinite dt | Malformed inputs fail before indexing/parallel execution |
| Interface behavior | Validate initial-state size before overrides; reject unknown indicator operations | Prevents invalid memory indexing and silent fallback |
| Interface behavior | Consistent beginning-of-increment stepTime/totalTime across hosts | Time-dependent custom routines need revalidation; built-in material response does not depend on these times |
| Verification | Actual Fortran probe for component order, history, padded batches and timestamps | Probe is test-only, not a material model |
| Numerical audit | New workToHeat=1e6 configuration and separate point/PD/FEM runs | Historical JC properties/archive untouched; not full thermal validation |
| Documentation/workflow | Module contracts, parameter/ABI limits, pinned PeriLab comparison, release-check runner | Existing source grouping and public entry points preserved |

## Reviewer-to-evidence map

| Comments | Code/document locations | Evidence / remaining work |
|---|---|---|
| 1, 5: usefulness and architecture | docs/module-contracts.md; src/material; tools/verify_release.py | Public adapter exercised independently of neighborhoods; no claim of measured usability superiority |
| 2: governing equation | docs/governing-equations.md | Equation-first explanation retained; manuscript editing remains separate |
| 3: reproducible benchmark | unpack_archive.py; archive_hashes.json; docs/quickstart.md | Fresh-clone candidate verification with logs; Zenodo not published |
| 4: stabilization | PdCorrespondenceSkeleton.cpp; correspondence_sampling test | No stabilization added; diagnostic is local and not a global eigenmode proof |
| 6, 7: captions and Figure 6 | docs/publication-assets.md | Separate manuscript/plotting task; no solver changes claimed to address it |
| 8: math libraries | eigen_rotation_test; docs/matrix-library.md | Optional Eigen oracle retained; no unmeasured speed claim or algorithm replacement |
| 9, 10: H8/FEM | benchmark README; docs/module-contracts.md | Independent included full-integration H8 verification solver, not Abaqus |
| 11: timestep | docs/time-step.md | Actual Verlet/user-step policy and existing fixed-space temporal sequence; no new stability bound claimed |
| 12: parameters and interface | docs/parameters.md; docs/interface.md; config_rejection/interface_contract/jc_thermal_units tests | Negative/configuration and Fortran ABI checks; arbitrary external VUMATs remain unverified |
| 13: PeriLab/Impact/repository | docs/perilab-comparison.md; docs/publication-assets.md | Official source pinned; external interfaces already exist; deposit checklist prepared but not executed |

## Explicit remaining numerical limits

No zero-energy stabilization, general finite-rotation proof, calibrated adiabatic
thermoplasticity or fracture verification is supplied. The heat-unit test isolates
one conversion; it does not fix legacy specific-energy units or plastic-work
approximation. Initial corotational state assumes identity stretch/rotation and is
not a general restart/polar-decomposition facility. Predefined fields and coordinates
are zero-filled, effective-modulus dtArray extensions and global Abaqus queries are
unsupported. New material routines require their own tests.

The 2026-10-31 timestep evidence was archived, not new space/time calculations.
The 2026-11-03 follow-up separately records new fixed-space coarse-grid temporal
runs. Neither is a full publication-matrix rerun or a proven stability bound.
New solves and archive reanalysis are listed separately in the validation reports.
