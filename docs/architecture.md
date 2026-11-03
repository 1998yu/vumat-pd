# Execution path
The regional case runner reads the JSON configuration and constructs reference nodes, volumes and neighborhoods in src/host. The PD kernel reconstructs the deformation gradient from reference and current bonds. The corotational update supplies the strain and rotation measures used by the material adapter.

VumatPdCorrespondenceMaterial owns per-point material history. VumatMaterialAdapter packs material points into blocks and calls the Fortran bridge and Johnson-Cook plugin. See [interface conventions](interface.md) for component ordering, units and stress frames. The PD host rotates stress back to the spatial frame and assembles reference force density. The integrator divides force density by reference density to obtain acceleration. Integrating force density over reference volumes gives the net internal force.

The independent H8 solver in benchmarks/tensile3d/src shares the material adapter and plugin. Its element integration and dynamics provide a separate numerical reference.

## Find the code

| Responsibility | Source |
|---|---|
| Read the case and select a host | [Case runner](../src/apps/main_pd_case_runner.cpp) and [configuration](../src/config/DemoConfig.cpp) |
| Advance one PD increment | [Boundary-value host](../src/host/BoundaryValueCaseRunner.cpp) |
| Reconstruct deformation and assemble internal force density | [PD correspondence](../src/pd/PdCorrespondenceSkeleton.cpp) |
| Update the rotating frame and transfer stress and history | [Corotational kinematics](../src/pd/CorotationalKinematics.cpp) and [material coupling](../src/material/VumatPdCorrespondenceMaterial.cpp) |
| Pack blocks and invoke the Fortran routine | [Material adapter](../src/material/VumatMaterialAdapter.cpp) and [Fortran bridge](../materials/bridge/) |
| Compare with the independent H8 formulation | [FEM benchmark driver](../benchmarks/tensile3d/src/main_jc_3d_fem.cpp) |

## Module inputs, outputs and ownership

| Module / entry | Inputs | Outputs / owned state | Dependencies and limits |
|---|---|---|---|
| config: DemoConfig, MaterialConfigJson | Case/material JSON | Validated configuration values | Paths currently resolved from working directory; plugin basenames from executable directory |
| host: Geometry | Grid counts, h, delta in mm | Reference nodes, neighbors, quadrature volumes in mm3 | Shape/force routines consume these; body volume uses boundary half weights |
| pd: PdCorrespondenceSkeleton | Reference geometry, displacement; spatial Cauchy stress in MPa | Dimensionless F; force density in N/mm3 | Reference K inverse cached; no zero-energy stabilization |
| pd: CorotationalKinematics | F old/new, left V, R, dt in ms | Dbar in 1/ms, spin increment, new V/R | Finite-difference velocity gradient; not a general finite-rotation accuracy proof |
| material: VumatPdCorrespondenceMaterial | Per-node F histories | Owns per-node corotational stress, state, temperature, energies | Only corotational host mode; transient spatial stress returned for force assembly |
| material: VumatMaterialAdapter | Local point/block arrays | Unpacked stress and state/temperature/energy results | Owns plugin handle and scratch memory; no neighborhoods; no caller-global thread safety |
| materials/bridge | C-compatible arrays | Calls the supplied Fortran VUMAT signature | Limited dtArray and zero-filled fields; see interface.md |
| damage: PdSimpleDamage | Raw stress/strain/history, geometry | Effective stress and bond attenuation | Does not rewrite VUMAT history; no validated fracture-neighborhood update |
| pd: ExplicitTimeIntegrator | Force density, solver density, dt | Displacement, velocity and acceleration | Velocity-Verlet; user-chosen dt; no automatic stability bound |
| host: BoundaryValueCaseRunner | Validated configuration and modules | Time histories, nodal fields and metadata | Owns order of operations and boundary constraints |
| apps | CLI arguments | Dispatches the configured host | No alternate material law hidden in CLI |

Host solver units are mm-ms-N-MPa. VUMAT material density/properties retain their
declared contract; no implicit general-purpose unit conversion is performed.
State rows belong to physical discretisation nodes. Padded rows are duplicated
scratch inputs and their returned values are discarded.

The adapter is a public C++ class in the static library and can be called without
a PD neighborhood, as the interface probe and material-point test demonstrate.
The current build still packages it with the host library; separate target
packaging is not claimed.
