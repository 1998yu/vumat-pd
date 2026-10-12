# Interface conventions and applicability

A routine with a different frame, component order or signature needs an adapter change and verification. VUMAT–PD provides one documented calling contract.

| Input | Current convention | Integration consequence |
|---|---|---|
| Symmetric arrays | 11, 22, 33, 23, 13, 12 | Standard Abaqus order is 11, 22, 33, 12, 23, 31; component-specific routines need permutation |
| Shear strain | Tensor shear | Convert routines expecting engineering shear |
| Deformation gradient | 11, 22, 33, 12, 23, 31, 21, 32, 13 | Preserve the declared nonsymmetric order |
| Stretch | Left stretch V | Standard Abaqus VUMAT expects right stretch U; transform inputs if the routine uses them |
| Stress | Corotational Cauchy stress | The host rotates it spatially and assembles its own force state |
| State | Persistent material-point rows | Configure state count and initialization |

The isotropic JC update does not use the stretch or deformation-gradient arrays and treats shear entries identically. Its FEM–PD agreement therefore does not verify these mappings for an arbitrary anisotropic or stretch-dependent routine.

## Batches and plugin entry

Spatial coupling uses at most 256 points per block. Rows retain point ownership. The last block is padded by duplicating its final valid row and discards padded outputs. Properties, old state, stress and energy histories pass through the adapter.

The Fortran wrapper uses the recent `jInfoArray/dtArray` signature through a C-compatible entry. The included Abaqus shims permit standalone example builds; no commercial solver is required.

## Units and thermal history

The spatial benchmark uses mm, ms, N and MPa, with solver density 0.0078 N·ms²/mm⁴. JC thermal inputs use their documented SI values. The work-to-heat multiplier is 1 and does not supply the MPa-to-J/m³ conversion needed for a physical temperature increment. Treat the temperature state as transferred numerical history, not validated adiabatic heating.

## Verified scope

Comparisons verify the included JC routine in documented 3D nonfracturing tensile models. Core tests cover affine response, neighbor search, an H8 patch, corotational updates and basic rigid-wall contact. Local maximum plastic strain remains resolution-sensitive.

General finite-rotation accuracy, arbitrary VUMAT compatibility, damage–neighborhood coupling, deletion and production impact/fracture need separate evaluation. Contact and damage source code is not application-validation evidence.

## Threading

Start custom routines with one thread. Before concurrent blocks, audit mutable saved/global state and external libraries. The JC plugin is compiled with recursive local Fortran storage; its tested path does not establish thread safety of new routines.
