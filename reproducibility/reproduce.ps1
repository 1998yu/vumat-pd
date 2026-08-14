$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Repo "build"

cmake -S $Repo -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build $Build --parallel
ctest --test-dir $Build --output-on-failure

Push-Location $Repo
try {
  & "$Build/bin/vumat_pd_run.exe" reproducibility/cases/jc_2d_crosssolver_coarse.json
  & "$Build/bin/vumat_pd_run.exe" reproducibility/cases/jc_2d_crosssolver_fine.json
  & "$Build/bin/jc_2d_fem.exe" reproducibility/configs/johnson_cook.json build/bin 2.0 0.00005 2400 reproducibility/generated/jc_2d_fem_coarse
  & "$Build/bin/jc_2d_fem.exe" reproducibility/configs/johnson_cook.json build/bin 1.0 0.000025 4800 reproducibility/generated/jc_2d_fem_fine
  python reproducibility/scripts/compare_jc_2d_crosssolver.py
  python reproducibility/scripts/verify_reproduction.py
  python reproducibility/scripts/plot_jc_2d_crosssolver_contours.py
} finally {
  Pop-Location
}
