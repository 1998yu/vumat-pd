# Interface conventions and applicability

A routine with a different frame, component order or signature needs an adapter change and verification. VUMAT–PD provides one documented calling contract.

Material JSON can explicitly declare `tensorConvention: legacy_vumat_pd` and
`stressFrame: corotational` or `spatial`. Omitted values retain the historical
legacy/corotational defaults; they do not auto-detect a routine. No other tensor
convention is currently implemented. The supplied Neo-Hookean configuration declares
spatial stress and is rejected by the generic JSON case hosts. Use its dedicated
spatial host. Existing Elastic/JC inputs retain their numerical contract.

Rebuild C++ callers against the new headers: the configuration struct has new fields.
Use named member assignments rather than positional aggregate initialization.
No stable binary ABI for the C++ configuration struct is promised.

`kinematicsMode: legacy_small_strain` is now rejected: the former implementation
changed initialization but still used the corotational update. It was not an
implemented alternate solver. The enum is retained for source compatibility and
rejected by the bridge constructor as well as the JSON loader.

| Input | Current convention | Integration consequence |
|---|---|---|
| Symmetric arrays | 11, 22, 33, 23, 13, 12 | Standard Abaqus order is 11, 22, 33, 12, 23, 31; component-specific routines need permutation |
| Shear strain | Tensor shear | Convert routines expecting engineering shear |
| Deformation gradient | 11, 22, 33, 12, 23, 31, 21, 32, 13 | Preserve the declared nonsymmetric order |
| Stretch | Left stretch V | Standard Abaqus VUMAT expects right stretch U; transform inputs if the routine uses them |
| Stress | Corotational Cauchy stress | The host rotates it spatially and assembles its own force state |
| State | Persistent material-point rows | Configure state count and initialization |

The isotropic JC update does not use the stretch or deformation-gradient arrays and treats shear entries identically. Its FEM–PD agreement therefore does not verify these mappings for an arbitrary anisotropic or stretch-dependent routine.

The Neo-Hookean bending/torsion demo uses a separate contract: spatial deformation gradients and spatial Cauchy stress pass directly through `VumatMaterialAdapter`. It bypasses the corotational coupling wrapper. The supplied Neo-Hookean routine matches the adapter's tensor ordering; its material-point tests cover all three shear planes and rotated finite stretch. Do not feed this spatial-stress routine into the corotational host unchanged.

## Batches and plugin entry

Supplied hosts now pass stepTime and totalTime at the start of the increment;
output histories retain end-of-increment times. Regional PD, FEM and the spatial
demo formerly passed end time, unlike the affine/material-point routes. This
changes time-dependent user routines and requires their revalidation. Included
Elastic/JC/Neo-Hookean routines do not use these two time arguments for response.
The probe verifies distinct step/total times arrive unchanged through the bridge.

Corotational spatial coupling uses at most 256 points per block. Rows retain point ownership. The last block is padded by duplicating its final valid row and discards padded outputs. The dedicated Neo-Hookean host calls the adapter directly for its particle block. Properties, old state, stress and energy histories pass through the adapter in both routes.

The Fortran wrapper uses the recent `jInfoArray/dtArray` signature through a C-compatible entry. The included Abaqus shims permit standalone example builds; no commercial solver is required.

This is a limited standalone ABI: only dtArray(1) is provided; effective-modulus
dtArray extensions are unsupported. Coordinates and predefined fields are zero-filled.
jInfoArray labels are block-local, including padded rows, not global mesh labels.
The wrapper does not implement annealing, global-element queries or Abaqus service
callbacks. The supplied examples use only supported fields. Materials relying on
unsupported fields require an explicit adapter extension and tests.

The relative-spin array uses the historical [23,31,12] extraction. This, the
symmetric shear order and left stretch are tested as the current contract, not
certified as standard Abaqus interchange. `tests/fixtures/ContractProbe.f90` is
a test-only component-sensitive probe through the actual wrapper; it is not a
physical material. Tests cover 1/255/256/257/513 rows, distinct shear coefficients,
F/stretch/spin packing, two-step history, temperature/energy return and host padding.

## Units and thermal history

The spatial benchmark uses mm, ms, N and MPa, with solver density 0.0078 N·ms²/mm⁴. JC thermal inputs use their documented SI values. The work-to-heat multiplier is 1 and does not supply the MPa-to-J/m³ conversion needed for a physical temperature increment. Treat the temperature state as transferred numerical history, not validated adiabatic heating.

`configs/johnson_cook_heat_units.json` isolates a workToHeat=1e6 conversion.
It is an audit configuration, not a replacement of the frozen benchmark. See
[the thermal audit](thermal-audit.md) for remaining energy/work limitations.

## Verified scope

Comparisons verify the included JC routine in documented 3D nonfracturing tensile models. Core tests cover affine response, neighbor search, an H8 patch, corotational updates and the supplied PD demos. Local maximum plastic strain remains resolution-sensitive.

The dedicated Neo-Hookean examples demonstrate 150° bending and 540° torsion with dynamic relaxation, but have no independent spatial reference or convergence study. General finite-rotation accuracy, arbitrary VUMAT compatibility, damage–neighborhood coupling, deletion and production impact/fracture need separate evaluation. The optional outer damage layer is retained separately from the VUMAT update; see [damage configuration](outer-damage.md). The supplied benchmark and demos disable it. The removed rigid-wall contact extension is not part of the current runtime.

## Threading

Start custom routines with one thread. Before concurrent blocks, audit mutable saved/global state and external libraries. The JC plugin is compiled with recursive local Fortran storage; its tested path does not establish thread safety of new routines.

## Case inputs and legacy output fields

Case output configuration selects a directory; the runners always write CSV and metadata. The former `csv`, `svg` and `metadata` switches were unused and have been removed. SVG generation belongs to the Python particle renderer.

The optional outer `damage` configuration is supported; its default is `none`. This host layer is separate from material state variables and the Johnson–Cook plastic update. Existing CSV layouts retain `simple_damage`/`damage` diagnostics and raw/effective-stress columns. They are zero/equal for the supplied nonfracturing cases and reflect host-level degradation when configured. See [the damage guide](outer-damage.md).
