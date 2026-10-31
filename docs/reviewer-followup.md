# Reviewer follow-up candidate (base 7538485)

This revision is developed on the local reviewer-interface-audit-20261031 branch
and has not been pushed. The previous GitHub run passed compilation/tests but failed metadata hash
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

The current timestep studies are archived evidence, not new space/time convergence
calculations performed for this candidate. Full publication-matrix reruns are not
claimed. New solves and archive reanalysis are listed separately in the validation report.
