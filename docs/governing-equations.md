# From the continuum equation to the implementation

PD here is a continuum model with meshfree nodal quadrature. Historical C++ names
such as `ParticleBlock` and the CSV column `particle` mean discretisation nodes.
They are retained for API/file compatibility.

In reference coordinates X, with current position y=X+u, the internal-force equation is

$$\rho_0(X)\ddot{u}(X)=\int_{H_X}[T_X(\xi)-T_{X'}(-\xi)]\,dV_{X'}+b(X),\quad \xi=X'-X.$$

The supplied boundary-value host imposes displacements and has no body-force input.
For its undamaged correspondence discretisation,

$$K_i=\sum_{j\in H_i}\omega_{ij}\xi_{ij}\otimes\xi_{ij}V_j,\qquad
F_i=\left[\sum_{j\in H_i}\omega_{ij}(y_j-y_i)\otimes\xi_{ij}V_j\right]K_i^{-1}.$$

The implementation uses omega=1 and a reference spherical horizon. Neighborhood
construction includes a scale-aware floating-point tolerance at the horizon.
Cartesian quadrature uses h cubed at interior nodes and half weights per exterior
coordinate face. It does not include spherical partial-cell or additional surface
corrections. Shape tensors use the available truncated neighborhood.

The material bridge converts F histories into corotational rate, spin and stretch
inputs, calls the configured plugin and recovers spatial Cauchy stress sigma.
See [the interface contract](interface.md) before substituting a material.

$$P_i=\det(F_i)\sigma_i F_i^{-T},\qquad T_i(\xi)=\omega_{ij}P_iK_i^{-1}\xi,$$
$$f_i^{\mathrm{int}}=\sum_{j\in H_i}[T_i(\xi_{ij})-T_j(-\xi_{ij})]V_j.$$

The optional damage path multiplies each interaction by (1-d_ij). It does not
reconstruct F or K on a broken neighborhood and is not validated fracture physics.
The vector returned by `computeForceDensity` is force per reference volume;
acceleration is f/rho, while the net internal force is sum(f_i V_i).
Small net internal force checks global balance, not local equilibrium or accuracy.

## Read the code in this order

1. `src/host/Geometry.cpp`: reference nodes, volumes and neighbors.
2. `src/pd/PdCorrespondenceSkeleton.cpp`: K, F and force assembly.
3. `src/pd/CorotationalKinematics.cpp`: finite-increment kinematics.
4. `src/material/VumatPdCorrespondenceMaterial.cpp`: material history and frames.
5. `src/material/VumatMaterialAdapter.cpp` and `materials/bridge/`: array ABI.
6. `src/host/BoundaryValueCaseRunner.cpp`: full velocity-Verlet increment.

## Zero-energy modes: current limitation

No hourglass force, bond-associated stabilization or other zero-energy compensation
is implemented. Neither rigid-body constraints, affine consistency, optional damage
nor the demonstrator's relaxation substitutes for such a term. Remove any manuscript
statement claiming it is present.

`correspondence_sampling` checks exact body volume and affine gradients, then
demonstrates that an alternating displacement can be invisible to the reconstructed
gradient at 27 symmetric interior nodes. This is a local sampling diagnostic, not
a global eigenmode calculation or a proof that a particular benchmark is unstable.
A future stabilization requires its own formulation, energy/force checks and
recomputed benchmark results.
