# Input parameters and units

Run from the checkout root. Case material paths and output directories are resolved
relative to the working directory; plugin basenames are resolved beside the runner.
Examples are runnable inputs; demos provide launch/render instructions; benchmarks
contain the reference matrix, archived data and comparison definitions.

## Regional PD case JSON

| Object / fields | Meaning and units |
|---|---|
| schemaVersion, name, type | Schema 1.0; case identifier; vumat_pd_boundary_case for free-interior PD dynamics |
| classification, scope, checks | Descriptive labels only; checks strings are not executable assertions |
| geometry.type | patch: Cartesian meshfree discretisation |
| geometry.nx, ny, nz | Numbers of nodes, not elements; extent is (n-1)h in each direction |
| geometry.spacing, horizon | h and delta in mm; boundary cases require nz>=3 and delta/h>=1.5 |
| time_integration.steps, dt, totalTime | Positive increment count, increment in ms, duration=steps*dt |
| boundary_conditions.type | regional_smooth_displacement |
| component, supportRegion, loadRegion | x/left/right or y/bottom/top |
| regionLayers | Number of constrained nodal planes; physical band width=(regionLayers-1)h |
| amplitude, riseTime | End displacement in mm; half-cosine rise duration in ms |
| planeStrain, rigidBodySuppression | Thickness displacement constraint; required rigid-body pins |
| dynamics.density | Solver mass density in N ms^2/mm^4; kg/m^3 multiplied by 1e-6 |
| outputEvery, snapshotEvery | Positive output intervals in increments |
| energyToleranceRelative | Acceptance threshold for final abs(K+U-W)/max(abs(W),abs(U)+K,1e-12) |
| netForceTolerance | Maximum norm of sum(f_i V_i), in N |
| minimumPlasticStrain | Required maximum observed equivalent plastic strain; dimensionless |
| kineticToInternalLimit | Acceptance threshold for final K/max(abs(U),1e-12) |
| observation.axialMin, axialMax | Optional reference-coordinate limits along loading axis, mm; defaults to inner band edges. Must be nodal planes within the domain |
| output.directory | Destination for CSV and metadata; rerunning can overwrite that destination |
| units | Boundary host accepts mm, ms, N, MPa and N*ms^2/mm^4 |

Observation limits affect only reported means, not loading, constitutive response or
forces. Interior end planes receive half weights; exterior end planes keep their
existing nodal volumes. Publication cases use 2 <= x <= 98 mm.

For affine/material-point checks use type vumat_pd_demo_case or
vumat_material_point_case, affine_displacement and mode uniaxial_xx,
simple_shear_xy or plane_strain_x_uniaxial_stress. maxStrain is dimensionless.
These prescribe a deformation history rather than solve free-interior dynamics.
The tolerance fields deformationGradientAbsolute, referenceStressAbsolute and
netForceAbsolute are respectively dimensionless, stress units and force units.

## Material entry and plugin configuration

The case's materials array supplies name, materialConfigPath, kinematicsMode,
elasticE (MPa), elasticNu, referenceTemperature (K), optional initialState or
zero-based initialStateOverrides, and equivalentPlasticStrainIndex (-1 disables).
internalIndicator has zero-based indices, reduction (max or min) and transform
(identity or one_minus); these are model-specific observers.
The legacy_small_strain selector is historical and is not a separate verified
dynamic boundary solver; use corotational for the supplied tensile cases.

The separate material file declares name, cmname (Fortran material identifier),
plugin or pluginPath, symbol/symbolName (default vumat_adapter_call), ndir=3,
nshr=3, nstatev, nfieldv, density and ordered props. density passed to the supplied
VUMAT is in kg/m^3, distinct from dynamics.density. Property/state layout is
material-specific; do not infer it from an array length.

| JC property (Fortran 1-based) | Meaning / supplied value |
|---|---|
| 1, 2 | E=210000 MPa; nu=0.3 |
| 3, 4 | A=350 MPa; B=500 MPa |
| 5, 6, 7 | Rate coefficient C=0.025; hardening exponent n=0.36; thermal exponent m=1 |
| 8 | Reference rate=1 per ms (the routine receives dt in ms) |
| 9, 10 | Tref=293 K; Tmelt=1793 K |
| 11, 12, 13 | rho=7800 kg/m^3; beta=0.9; Cp=500 J/(kg K) |
| 14 | workToHeat=1 in the archived configuration |

JC states (Fortran 1-based): initialized, equivalentPlasticStrain, temperature,
yieldStress, lastPlasticIncrement, cumulativeIterations. Thus its equivalent plastic
strain index in JSON is 1. The supplied elastic material uses E,nu; the dedicated
Neo-Hookean demo has its own documented spatial-stress contract.

**Unresolved thermal convention:** MPa plastic-work density needs a factor of 1e6
to become J/m^3 with SI rho/Cp. The archived workToHeat=1 does not supply it.
This revision does not silently recalibrate the published material. It must not be
described as validated physical adiabatic heating; changing it requires new runs.

See [outer damage](outer-damage.md) for model/mode, initiation, failure,
eqpsStateIndex and deletionEnabled. All publication inputs disable this path.

## Neo-Hookean spatial demo

Its four properties are shear modulus mu=769.230769 MPa, bulk modulus
K=1666.666667 MPa, shear viscosity etaShear=0 and bulk viscosity etaBulk=0
(viscosities in MPa ms). The zero viscosities select the purely elastic material.
These are separate from numerical relaxation used by the large-motion demo host.
Use the dedicated vumat_pd_large_motion_3d route described in the demo guide;
selecting this plugin in a corotational tensile input is not an equivalent setup.
