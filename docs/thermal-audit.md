# JC thermal-unit audit

The frozen JC configuration is preserved. In the routine, stress and plastic work
per unit volume are expressed in MPa, while rho=7800 kg/m3 and Cp=500 J/(kg K)
are SI values. Thus the temperature update needs 1e6 J/m3 per MPa. The archived
workToHeat=1 omits that factor.

The isolated audit configuration sets only property 14 to 1e6. The first-plastic-step
test keeps strain, previous temperature and stress identical, verifying identical
first-step stress and a millionfold temperature-increment ratio. Subsequent stress
can change through thermal softening, so later response must be recomputed.

Run the material-point and coarse spatial audit with:

```bash
python benchmarks/tensile3d/run_thermal_audit.py
```

The audit runner derives the material-point input from examples/jc_material_point.json
instead of maintaining another copy of its loading parameters. It also derives
the coarse PD input from pd_coarse.json and runs an independent coarse H8 FEM case
with the same converted configuration. It writes separate generated/thermal_audit
outputs. It never replaces raw/, expected/ or the original material file.

## What this does not fix

- The archived VUMAT internal/inelastic energy outputs divide MPa work by SI density
  without converting to J/kg. Applying workToHeat changes temperature only.
- The internal-work expression uses a simple six-component sum; tensor shear work
  needs its own factor-two audit for non-normal loading.
- The existing plastic-work approximation is 0.5 times plastic increment times
  updated equivalent stress. Its accuracy and constitutive calibration are not
  established by a unit conversion.
- The reference rate is 1/ms in the current time convention; a physical calibration
  expressed in 1/s needs a separate conversion.

Therefore this audit is not a newly validated thermomechanical implementation.
The publication matrix remains the historical mechanical/interface benchmark,
with explicit limits on temperature and energy claims. A physical thermal revision
must resolve work/energy definitions and rate calibration, then regenerate all
affected publication comparisons. Do not overwrite the old baseline to conceal changes.
