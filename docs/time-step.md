# Time increment and numerical sensitivity

The regional PD host uses velocity-Verlet: displacement prediction, prescribed
boundary motion, constitutive/force update, velocity correction, and reapplication
of constraints. `time_integration.dt` is supplied by the user in milliseconds;
the code does not compute or guarantee a critical stable increment.

For a linear conservative semidiscrete system with positive mass and stiffness,
the central-difference bound is dt <= 2/omega_max. The present nonlinear VUMAT host
does not assemble that spectrum. A continuum estimate h/c_p, with
c_p=sqrt(E(1-nu)/(rho(1+nu)(1-2nu))), can screen an input but is not a proven PD
stability bound: neighborhood quadrature, constraints and material tangent matter.
Use solver density in N ms^2/mm^4 and E in MPa in that estimate.

The archived tensile study uses 50, 25 and 12.5 ns PD runs at fixed h=1 mm and
delta=3 mm. The baseline is 25 ns = 0.000025 ms. The benchmark also includes
an FEM 12.5 ns check. These assess sensitivity of monitored histories and fields,
not unconditional stability, an asymptotic convergence order, or suppression of
zero-energy modes.

To repeat the PD temporal sequence:

```bash
python benchmarks/tensile3d/run_cases.py --cases pd_dt50 pd_shared pd_dt12p5 --threads 2
python benchmarks/tensile3d/analyse_and_plot.py --results benchmarks/tensile3d/generated/results
```

The analyzer needs the other reference cases as well; run the full matrix before
the second command if generated/results is incomplete. Do not combine old and new
results without recording the binary revision and configuration. For a new material,
geometry or loading duration, repeat temporal refinement and check finite response,
energy/work histories and the outputs of interest. Passing a CTest case means its
specified acceptance criteria passed, not that convergence has been established.
