# VUMAT–PD

**Reuse existing material routines in correspondence peridynamics.**

VUMAT–PD connects compiled Fortran VUMAT-style routines to a standalone C++ PD solver. The interface handles runtime plugin loading, tensor mapping, block-wise constitutive updates and persistent material history. The PD host handles particle neighborhoods, nonlocal kinematics, internal forces and explicit motion. The supplied examples run without Abaqus.

The project implements the interface described in *VUMAT-PD: A plugin-based interface for VUMAT-driven peridynamic correspondence simulations*. Its central capability is constitutive reuse: integrate a compatible material routine without rewriting its material algorithm inside the PD solver.

[Architecture](docs/architecture.md) · [Add a material](docs/add-a-vumat.md) · [Interface contract](docs/interface.md) · [PD particle demos](demos/README.md) · [Verification](benchmarks/tensile3d/README.md)

## How the coupling works

![Constitutive coupling from PD particle kinematics through the VUMAT interface and back to PD forces](docs/paper-figures/coupling-workflow.png)

*The coupling workflow from the manuscript: nonlocal PD kinematics → objective material inputs → compiled VUMAT plugin → spatial stress recovery and PD force assembly.*

The plugin receives material-point arrays rather than neighborhoods or bonds. The coupling layer gathers particle data into blocks, applies the declared tensor and frame conventions, invokes the material routine and returns updated stress, state and energy. Each particle retains its material history across increments.

## What you can use

| Capability | Project support |
|---|---|
| Reusable constitutive plugins | Select a compiled Fortran material library and its properties through configuration. |
| Block-wise material updates | Maintain point-to-row ownership, tensor ordering and state transfer through the shared adapter. |
| Correspondence PD simulation | Reconstruct deformation measures, recover spatial stress and assemble nonlocal particle forces. |
| Different material paths | Elastic and Johnson–Cook use the corotational coupling route; the supplied Neo-Hookean demos use a dedicated spatial-stress route. |
| Optional outer damage | Retain pointwise degradation and critical-stretch bond attenuation outside the VUMAT update. See [damage configuration](docs/outer-damage.md). |
| Reproducible verification | Material-point checks, coupling checks, archived 3D comparisons and parameter-sensitivity studies. |

Start with [the material integration guide](docs/add-a-vumat.md). The [interface contract](docs/interface.md) explains shear ordering, left/right stretch and stress-frame conventions that must be mapped explicitly for another routine.

## Material-transfer verification

The manuscript tests the same Johnson–Cook plugin in independent 3D PD and H8 FEM formulations. Both use a 100 × 20 × 4 mm body and a smooth 2 mm end displacement over 0.12 ms. FEM provides an independent verification reference; the project runtime and all four demo simulations use PD.

![Fine-resolution PD and FEM verification of axial stress and equivalent plastic strain](docs/paper-figures/fine-fields.png)

*Fine-resolution fields from the manuscript. The two formulations reproduce similar stress and plastic-strain distributions; the local plastic-strain maximum remains resolution-sensitive.*

| Matched fine-resolution comparison | Result |
|---|---:|
| Final mean axial stress difference | 0.17% |
| Stress-history NRMSE | 0.55% |
| Displacement RMSE / imposed displacement | 0.96% |
| Maximum local plastic-strain difference | 11.52% |

These results support constitutive-transfer consistency for the documented nonfracturing JC case. They do not establish universal VUMAT compatibility or full convergence.

<details>
<summary>Geometry, verification pathway and coarse-grid fields</summary>

![Three-dimensional tensile geometry and prescribed loading history](docs/paper-figures/benchmark-setup.png)

*Fixed 100 × 20 × 4 mm geometry, free lateral/thickness faces and 2 mm end bands.*

![Common material interface with independent FEM and PD kinematics and force assembly](docs/paper-figures/verification-path.png)

*Shared material input and plugin; independent kinematic reconstruction and internal-force assembly.*

![Coarse-resolution PD and FEM verification fields](docs/paper-figures/coarse-fields.png)

*Coarse comparison at h = 2 mm and PD horizon = 6 mm. These are verification fields, separate from the discrete-particle demo views.*

</details>

## Spatial and temporal sensitivity

The benchmark also varies PD grid spacing, horizon and time step against one finest H8 FEM numerical reference. Stress histories are more stable than local plastic-strain maxima; parameter refinement does not reduce every deviation monotonically.

<details>
<summary>View the manuscript's sensitivity results</summary>

![Grid, horizon and time-step sensitivity against the same finest FEM reference](docs/paper-figures/sensitivity.png)

*For the fine PD baseline, stress-history, displacement and maximum-plastic-strain deviations are 0.483%, 0.540% and 8.113%. These use the finest FEM reference and differ from the matched-pair metrics above.*

</details>

The [benchmark directory](benchmarks/tensile3d/README.md) preserves original data hashes, final case inputs, separate metric baselines and reproduction tools. Browse [all six manuscript figures](docs/paper-figures/README.md) and their source traceability.

## Explore the PD particle demos

Each demo folder contains its own setup, discrete-particle animation, final-state preview, rerun commands and provenance. Every colored point is an actual PD particle at its computed deformed position. Free interior particles evolve under PD forces.

| Demo | What it shows | Open |
|---|---|---|
| Elastic tension | Material reuse and transient elastic stress | [Demo folder](demos/elastic-tension/README.md) |
| Johnson–Cook tension | Elastoplastic stress and persistent material history | [Demo folder](demos/johnson-cook-tension/README.md) |
| Neo-Hookean 150° bending | Finite deformation through the dedicated spatial-stress route | [Demo folder](demos/neohookean-bending/README.md) |
| Neo-Hookean 540° torsion | Large end rotation and computed PD interior motion | [Demo folder](demos/neohookean-torsion/README.md) |

The Neo-Hookean demos extend the gallery beyond the paper's JC benchmark; they have no independent spatial FEM/experimental reference. All four gallery cases disable the optional outer damage layer.

## Optional outer damage

`SimpleDamageModel` and `PdSimpleDamage` remain available with their host calls and configuration. Point modes use material-state or principal-strain thresholds; the bond mode uses critical stretch. This host layer is separate from the constitutive routine's raw stress and persistent history. Its algorithmic checks cover degradation, irreversibility and bond failure; they do not validate physical fracture applications. Read [the damage guide](docs/outer-damage.md) before enabling it.

## Build and start

Requirements: CMake 3.25+, C++17, gfortran, Ninja and OpenMP. Builds are checked with Windows MSYS2 UCRT64 and Ubuntu GCC/gfortran. Use an ASCII checkout path on Windows and place compiler runtimes on `PATH`.

```bash
cmake --preset release
cmake --build --preset release --parallel
ctest --preset release
python demos/run.py all --threads 2
```

The launcher executes new PD simulations and records the actual programs, plugins and result hashes. Python 3.10+ is used for orchestration. Particle rendering requires the [demo dependencies](demos/requirements.txt) and FFmpeg.

For a PD runtime build without FEM or standalone verification executables:

```bash
cmake -S . -B build/pd-only -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/pd-only --parallel
```

## Scope and reuse

The demonstrated scope is material transfer under the documented interface conventions. Standard Abaqus shear ordering and stretch conventions require explicit mapping for affected routines. Additional materials and hosts need separate checks. Thermal calibration, general finite-rotation accuracy and production impact/fracture capability remain outside the paper's verified scope.

```text
src/                  PD hosts, material coupling and optional outer damage
include/              Public C++ interfaces
materials/            Fortran material routines and bridge
configs/              Material properties and plugin selection
examples/             PD inputs and small verification cases
demos/                Four particle-demo folders and shared rendering tools
benchmarks/tensile3d/  Independent validation inputs, data and metrics
docs/                 Integration guides and manuscript figures
tests/                Automated numerical checks
```

Generated runs and builds are ignored by Git. The project uses the [BSD 3-Clause License](LICENSE). For reproducible bug reports or integration questions, use [GitHub issues](https://github.com/1998yu/vumat-pd/issues) and the [contribution guide](CONTRIBUTING.md).
