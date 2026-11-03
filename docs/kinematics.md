# Corotational kinematics

The correspondence PD layer reconstructs the deformation gradient at every
material point. Consecutive deformation gradients define an approximate
velocity gradient

\[
L \approx \frac{F_{n+1}-F_n}{\Delta t}F_n^{-1}.
\]

The rate-of-deformation and spin tensors are

\[
D=\frac{1}{2}(L+L^T), \qquad W=\frac{1}{2}(L-L^T).
\]

The Flanagan–Taylor algorithm updates the left stretch \(V\) and rotation
\(R\). The constitutive rate passed to VUMAT is expressed in the corotational
frame:

\[
\bar D=R^TDR.
\]

The incremental rotation uses the matrix exponential of the skew material
spin, with a positive coefficient multiplying \(\Omega^2\):

\[
Q=I+\frac{\sin(\Delta t|\omega|)}{|\omega|}\Omega
 +\frac{2\sin^2(\Delta t|\omega|/2)}{|\omega|^2}\Omega^2,
\qquad R_{n+1}=QR_n.
\]

The half-angle form avoids cancellation at small increments. Regression
checks compare finite spins about all three axes with analytic rotations,
check orthogonality, determinant and isotropic-stress invariance, and follow
1000 accumulated increments. These checks verify rotation assembly; they do
not remove the time-discretization error in the reconstructed velocity gradient
or the explicit left-stretch update.

The adapter also supplies deformation gradients, old/new stretch, and the
relative-spin increment. VUMAT returns the stress in the corotational material
frame. The PD force assembly uses the spatial stress

\[
\sigma=R\bar{\sigma}R^T.
\]

## Why this is necessary

An infinitesimal-strain update mixes material deformation with rigid rotation.
That approximation is acceptable only when both strain and rotation are
small. The corotational update separates material deformation from rigid
rotation and is the current spatial host's default. Regression checks do not
establish accuracy for all finite rotations or impact/fracture applications;
those uses require independent verification.

## Current approximation

The compact solver obtains \(L\) from consecutive deformation gradients.
A production PD implementation may instead reconstruct \(L\) directly from
the nonlocal velocity field. That avoids differencing \(F\) and is an
important extension for high-rate applications.

## Matrix operations and Eigen verification

The production path uses fixed-size 3x3 arrays. Its Rodrigues expression evaluates
the exponential of a skew-symmetric material-spin matrix. Flanagan–Taylor also
determines that spin from stretch and updates stretch; a generic matrix exponential
does not replace those steps.

This revision adds an optional independent Eigen MatrixFunctions check. It compares
24 rotations (three coordinate axes and one oblique axis; zero, small, positive and
negative angles) with Eigen's matrix exponential. It leaves the production numerical
path and the runtime dependency set unchanged.

```bash
cmake --preset release -DVUMAT_PD_EIGEN_CHECK=ON -DEIGEN3_INCLUDE_DIR=/path/to/eigen-3.4.0
cmake --build --preset release
ctest --preset release -R eigen_rotation --output-on-failure
```

Eigen is not downloaded by CMake. Supply an Eigen installation/header tree explicitly.
This is a numerical cross-check, not evidence that the current implementation is
faster than Eigen. A production library/backend change needs matched accuracy and
timing measurements. The test does not validate the whole finite-rotation host.

Reference: [Eigen MatrixFunctions documentation](https://libeigen.gitlab.io/eigen/docs-nightly/unsupported/group__MatrixFunctions__Module.html).

## Governing equations and spatial discretisation

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

See the [architecture source map](architecture.md#find-the-code) for the functions that implement these equations.

## Zero-energy modes: current limitation

No hourglass force, bond-associated stabilization or other zero-energy compensation
is implemented. The optional damage layer attenuates bond forces and does not stabilize the correspondence discretisation.

`correspondence_sampling` checks exact body volume and affine gradients, then
demonstrates that an alternating displacement can be invisible to the reconstructed
gradient at 27 symmetric interior nodes. This is a local sampling diagnostic, not
a global eigenmode calculation or a proof that a particular benchmark is unstable.
A future stabilization requires its own formulation, energy/force checks and
recomputed benchmark results.
