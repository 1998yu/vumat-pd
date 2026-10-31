# Module inputs, outputs and ownership

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
| apps | CLI arguments | Calls host / dedicated spatial demo | No alternate material law hidden in CLI |

Host solver units are mm-ms-N-MPa. VUMAT material density/properties retain their
declared contract; no implicit general-purpose unit conversion is performed.
State rows belong to physical discretisation nodes. Padded rows are duplicated
scratch inputs and their returned values are discarded.

The adapter is a public C++ class in the static library and can be called without
a PD neighborhood, as the interface probe and material-point test demonstrate.
The current build still packages it with the host library; separate target
packaging is not claimed.

## One increment to read

BoundaryValueCaseRunner: predict displacement → impose boundary motion →
compute F → update material and save history → optional damage → force density →
correct velocity → reapply boundary motion → write observations.

Independent FEM verification lives in benchmarks/tensile3d/src and is built only
with BUILD_TESTING=ON. The H8 element has eight nodes, trilinear shape functions
in natural coordinates and full 2x2x2 Gauss integration.
