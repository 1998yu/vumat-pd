#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$repo/build"

cmake -S "$repo" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure

cd "$repo"
"$build/bin/vumat_pd_run" reproducibility/cases/jc_2d_crosssolver_coarse.json
"$build/bin/vumat_pd_run" reproducibility/cases/jc_2d_crosssolver_fine.json
"$build/bin/jc_2d_fem" reproducibility/configs/johnson_cook.json build/bin 2.0 0.00005 2400 reproducibility/generated/jc_2d_fem_coarse
"$build/bin/jc_2d_fem" reproducibility/configs/johnson_cook.json build/bin 1.0 0.000025 4800 reproducibility/generated/jc_2d_fem_fine
python3 reproducibility/scripts/compare_jc_2d_crosssolver.py
python3 reproducibility/scripts/verify_reproduction.py
python3 reproducibility/scripts/plot_jc_2d_crosssolver_contours.py
