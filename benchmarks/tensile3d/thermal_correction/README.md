# Plastic-work-to-heat conversion correction

This supplement records the four tensile simulations rerun on 2026-11-04 with the missing factor of 10^6 restored using configs/johnson_cook_heat_units.json (work-to-heat multiplier 1000000). Geometry, loading, spatial resolution and the 25 ns time increment match the original coarse and fine comparison cases. The raw metadata and execution records preserve the original run provenance, including local paths.

## Reproduce the comparison

From the repository root:

```sh
python -m pip install -r benchmarks/tensile3d/requirements.txt
python benchmarks/tensile3d/thermal_correction/compare.py
```

The script reads the original results from ../raw and the corrected results from this directory's raw folder. It writes comparison.json. expected_comparison.json contains the frozen comparison. The output should contain four individual cases and two matched PD-FEM comparisons.

| Case | Final mean axial stress change (%) | Maximum equivalent plastic strain change (%) |
|---|---:|---:|
| pd_coarse | -0.028055 | 0.195609 |
| fem_coarse | 0.027246 | 0.065363 |
| pd_shared (fine) | -0.035277 | 0.397618 |
| fem_fine | -0.105412 | 0.212599 |

Changes are relative to each case's original output. The fine PD-FEM stress-history NRMSE is 0.551131% after correction (original 0.551590%). It uses the peak PD mean stress for normalisation. The maximum absolute changes across the four cases round to 0.11% for stress and 0.40% for plastic strain.

## Files and scope

- raw: corrected stress/displacement histories, final field data and metadata for all four cases. CSV files are gzip-compressed.
- *.execution.json: commands, elapsed time and successful return codes recorded during the runs.
- compare.py: original comparison calculations with repository-relative paths and compressed-input support.
- expected_comparison.json: saved numerical results.
- sha256.json: checksums of the deposited raw data, execution records, comparison script and expected results.

The shared original benchmark archive remains the reference for the published sensitivity matrix. This supplement quantifies the mechanical effect of the heat-conversion correction in the four matched tensile cases. The field outputs do not include temperature, so the corresponding temperature entries in the comparison are null.
