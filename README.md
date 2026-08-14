# VUMAT–PD

VUMAT–PD is a research-oriented, solver-independent adapter that reuses
Abaqus/Explicit-style VUMAT material updates inside a correspondence
peridynamics (PD) solver.

The repository focuses on the interface between nonlocal PD kinematics and
block-wise constitutive updates. It includes a Flanagan–Taylor corotational
kinematics update, VUMAT tensor mapping, spatial stress rotation, an explicit
PD demonstration solver, and small reproducible verification cases.

> Status: research software. The repository is suitable for verification,
> method development, and VUMAT integration experiments. It is not a
> replacement for a production finite-element or peridynamics package.

## What is included

- Block-wise C++/Fortran VUMAT adapter.
- Node-level correspondence PD deformation-gradient and force-state assembly.
- Flanagan–Taylor update of rotation and left stretch.
- Corotational rate-of-deformation, relative spin, stretch, and deformation
  gradient transfer to VUMAT.
- Rotation of VUMAT stress back to the spatial PD frame.
- Explicit time integration, simple bond damage, and rigid-wall contact.
- JSON-driven material and case configuration.
- BSD-licensed linear-elastic example VUMAT.
- BSD-licensed Johnson-Cook example VUMAT and manuscript reproduction package.
- BSD-licensed Johnson-Cook example VUMAT and manuscript reproduction package.
- Unit and regression tests for tensor mapping, rigid rotation, neighbor
  search, contact, affine extension, and affine shear.

## What is not included

- Abaqus, Abaqus headers, or proprietary solver libraries.
- Third-party CDPM2, M7, or other restricted VUMAT source files.
- Manuscript source files or experimental datasets.
- MPI/distributed-memory execution.
- A complete production implementation of particle deletion and flying-point
  management.

Users are responsible for ensuring that any VUMAT they load may legally be
used and redistributed.

## Reproduce the manuscript results

The repository contains the complete Johnson-Cook source, coarse and fine PD
inputs, matching explicit FEM path, 241-time raw histories, field data,
post-processing scripts, expected values and tolerances. Start with
[`reproducibility/README.md`](reproducibility/README.md) or run
`reproducibility/reproduce.ps1` / `reproducibility/reproduce.sh`.

## Reproduce the manuscript results

The repository contains the complete Johnson-Cook source, coarse and fine PD
inputs, matching explicit FEM path, 241-time raw histories, field data,
post-processing scripts, expected values and tolerances. Start with
[`reproducibility/README.md`](reproducibility/README.md) or run
`reproducibility/reproduce.ps1` / `reproducibility/reproduce.sh`.

## Requirements

- CMake 3.20 or newer.
- A C++17 compiler.
- A Fortran compiler compatible with legacy fixed-form VUMAT sources.
- Ninja is recommended.

Tested locally with GCC/gfortran through MSYS2 UCRT64 on Windows. The GitHub
Actions workflow verifies GCC/gfortran on Ubuntu.

## Build and test

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

If Ninja or the compilers are not on `PATH`, configure explicitly:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_Fortran_COMPILER=gfortran
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

## Run an example

```bash
./build/release/bin/vumat_pd_run examples/affine_bar_elastic.json
./build/release/bin/vumat_pd_run examples/affine_shear_elastic.json
```

On Windows, use `vumat_pd_run.exe`.

Generated files are written below `examples/output/`, which is excluded from
Git.

## Kinematic coupling

The default material update follows

```text
PD displacement field
  -> deformation gradients F_n and F_n+1
  -> velocity gradient L
  -> D and W
  -> Flanagan–Taylor rotation R and left stretch V
  -> unrotated rate D_bar and relative spin
  -> block-wise VUMAT update
  -> corotational stress sigma_bar
  -> spatial stress sigma = R sigma_bar R^T
  -> PD force state
```

The legacy infinitesimal-strain mode remains available only for controlled
regression comparisons. Corotational kinematics is the recommended mode for
impact and finite-rotation problems.

## Use your own VUMAT

The example plugin target in `CMakeLists.txt` combines:

- a VUMAT source;
- `materials/bridge/vumat_c_wrapper_jinfo.f90`;
- `materials/bridge/abaqus_stubs.f90`.

Replace the example source with a VUMAT that you are permitted to use, then
create a matching JSON material configuration. See
[docs/add-a-vumat.md](docs/add-a-vumat.md).

## Repository layout

```text
include/             Public C++ interfaces
src/                 VUMAT–PD and demonstration-solver implementation
materials/bridge/    Standalone C/Fortran bridge and minimal Abaqus shims
materials/examples/  Redistributable example VUMAT
configs/             Material configurations
examples/            Small JSON verification cases
tests/               Fast unit and regression tests
docs/                Architecture, kinematics, and extension guides
```

## Known limitations

- The current velocity gradient is reconstructed from consecutive deformation
  gradients rather than directly from the nonlocal velocity field.
- Energy diagnostics still require a fully consistent work-conjugate update
  for large-deformation production studies.
- Fracture-related neighborhood degradation and particle deletion require
  further verification.
- The included solver is deliberately compact and is not distributed-memory
  parallel.

## License and citation

The repository is released under the BSD 3-Clause License. See
[LICENSE](LICENSE) and [CITATION.cff](CITATION.cff).

