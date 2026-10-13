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
