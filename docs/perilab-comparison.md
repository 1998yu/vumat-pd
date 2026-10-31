# Scope comparison with PeriLab

Audit date: 2026-10-31. PeriLab source pinned at
[6c2094fab28cba25d55c1bca2858a9c079cd9f67](https://github.com/PeriHub/PeriLab.jl/tree/6c2094fab28cba25d55c1bca2858a9c079cd9f67).
This is a source/documentation comparison, not an execution or performance comparison.

| Aspect | VUMAT-PD candidate | PeriLab evidence / status |
|---|---|---|
| External VUMAT material | Supported under an explicitly limited standalone ABI | Supported: Correspondence_VUMAT.jl registers the material and invokes the library |
| Material history | Supported; probe tests two steps and partial batches | Implemented fields for state, temperature and internal/inelastic energies |
| Runtime/host | Standalone C++ host with Fortran bridge; OpenMP blocks | Julia framework; VUMAT module uses Data_Manager and StaticArrays |
| FEM use | Separate included H8 verification executable sharing the adapter/plugin | VUMAT module fe_support() returns true; exact validation scope not independently checked here |
| Component conventions | Legacy shear/spin/left-stretch contract; component-sensitive tests | Source uses matrix_to_engineering_voigt, matrix_to_voigt and matrix_to_vector; equivalence requires checking helper definitions and paired tests, not assumed |
| Broad multiphysics and MPI | Not implemented by this small host | Advertised in the pinned project README; not benchmarked here |
| Stabilization | Not implemented in this correspondence kernel | No equivalence/completeness claim made by this limited VUMAT-module audit |
| Speed / universal compatibility | Not established | Not assessed by this audit |

Sources:
- [Pinned VUMAT module](https://github.com/PeriHub/PeriLab.jl/blob/6c2094fab28cba25d55c1bca2858a9c079cd9f67/src/Models/Material/Material_Models/Correspondence/Correspondence_VUMAT.jl)
- [Pinned project README](https://github.com/PeriHub/PeriLab.jl/blob/6c2094fab28cba25d55c1bca2858a9c079cd9f67/README.md)
- [Pinned VUMAT unit tests](https://github.com/PeriHub/PeriLab.jl/blob/6c2094fab28cba25d55c1bca2858a9c079cd9f67/test/unit_tests/Models/Material/Material_Models/Correspondence/ut_Correspondence_VUMAT.jl)

## Defensible software contribution

VUMAT-PD offers an inspectable C++/Fortran integration example with explicit material
contracts and a shared-material verification workflow. Its intended users are
developers adapting compatible routines into a small standalone host or studying
the coupling boundary. Existing VUMAT reuse in PeriLab must be acknowledged.
Language choice and repository size alone do not establish scientific novelty or
measured usability advantages. Neither a first-of-kind claim nor superiority over
PeriLab is supported by this audit.
