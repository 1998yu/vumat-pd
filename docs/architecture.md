# Architecture

VUMAT–PD is organized into four layers:

1. **PD kinematics** reconstructs deformation gradients from particle
   neighborhoods.
2. **Corotational kinematics** updates rotation, left stretch, unrotated
   rate-of-deformation, and relative spin.
3. **VUMAT adapter** maps C++ tensors and state vectors into an
   Abaqus/Explicit-style block call.
4. **PD force assembly** rotates constitutive stress to the spatial frame and
   evaluates the correspondence force state.

`VumatMaterialAdapter` is independent of the demonstration PD solver and is
the main reusable interface. `VumatPdCorrespondenceMaterial` owns the coupling
state required by the correspondence formulation.

The compact case runner is included to make the interface testable without
Abaqus or Peridigm. Larger solvers can replace the geometry, neighborhood,
time integration, and output layers while retaining the material adapter.

