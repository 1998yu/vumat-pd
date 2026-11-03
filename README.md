# VUMAT-PD
A standalone C++ correspondence-peridynamics host loads compatible Fortran material plugins at runtime. The interface defines tensor and state transfer. The paper benchmark exercises the same Johnson-Cook plugin in PD and an independent H8 FEM solver. See [architecture](docs/architecture.md), [material integration](docs/add-a-vumat.md) and [PeriLab comparison](docs/perilab-comparison.md).

## Run the minimal example

Use CMake 3.25+, Ninja, GNU C++/Fortran, OpenMP and Python. Run from the repository root.

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/bin/vumat_pd_run examples/jc_material_point.json
```

On Windows use vumat_pd_run.exe. The example runs a prescribed material-point history.

## Reproduce the paper

```sh
python -m pip install -r benchmarks/tensile3d/requirements.txt
python benchmarks/tensile3d/unpack_archive.py
python benchmarks/tensile3d/analyse_and_plot.py
python benchmarks/tensile3d/verify_metrics.py
python benchmarks/tensile3d/run_cases.py --cases fem_coarse pd_coarse
```

See the [benchmark guide](benchmarks/tensile3d/README.md) for the full matrix, tolerance definitions and Supplementary Movie S1.
