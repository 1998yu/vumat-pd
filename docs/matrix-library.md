# Matrix operations and Eigen verification

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
