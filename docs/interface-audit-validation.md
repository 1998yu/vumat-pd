# Interface audit validation — 2026-10-31

Base remote/main: 75384859fec2f6163ff7c1956c73635520218bc7.
Revision branch: reviewer-interface-audit-20261031. Validation was performed before
the local commit; this branch has not been pushed.
See [reviewer mapping](reviewer-followup.md) for scope and unresolved issues.

## Fresh-clone verification

A separate GitHub clone of the base received the uncommitted candidate source
overlay and started with an empty build/release. GNU C++/Fortran 16.1.0, Python
3.11.4 and optional Eigen 3.4.0 were used on Windows. No binary/build cache was
copied. Eigen headers were supplied as a verification dependency.

- 20/20 CTest cases passed, including the Fortran ABI probe, configuration
  rejection, JSON/CSV integrity policy and isolated thermal conversion.
- 14 numerical CSV outputs are byte-identical to the pre-revision baseline.
- All 41 archived CSV/metadata digests passed with the documented JSON newline rule.
- All 11 main-case archived metrics, matched FEM–PD pairs and FEM symmetry
  metrics reproduced with the original tolerances.
- All 59 checked C++/Fortran/build/test source files match the tested candidate
  after line-ending normalization.
- Script compilation and demo-entry help checks passed.

Actual logs, commands, exit codes, source/CSV checks and thermal results are in
[validation/interface-audit-20261031](../validation/interface-audit-20261031/).
Public logs replace machine-specific checkout/tool paths with named placeholders.
The detailed CTest log is retained; no summary is substituted for missing evidence.

The remote base's Linux compile/test steps succeeded, but its metadata hash stage
failed. That exact byte-normalization defect is fixed and negatively tested here.
The new Linux workflow has not run because the candidate is not pushed. No Linux
success is claimed. The unmodified remote commit still has its recorded CI failure.

## New solves versus archive analysis

New execution includes the CTest material/PD demonstrations, two material-point
histories (legacy and heat-unit audit), and converted coarse PD/FEM solves.
The 11-case publication matrix was reanalyzed from verified archives, not fully
solved again. The separate thermal audit did not replace those archives.

At the end of the material-point history, temperature changed from 293.000001047 K
to 294.046788721 K. At identical coarse spatial settings, the converted configuration
changed final mean stress by -0.02805% in PD and +0.02725% in FEM, and maximum
plastic strain by +0.19561% and +0.06536%, respectively (relative to each legacy
case). These are sensitivity observations, not a convergence or physical accuracy
claim. See [thermal audit limits](thermal-audit.md).

No zero-energy stabilization, new stable-step estimator, new spatial-convergence
study, general finite-rotation proof or validated fracture model was introduced.
Caption/Figure 6 edits and external archival publication remain separate.
