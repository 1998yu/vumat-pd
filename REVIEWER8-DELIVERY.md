# SoftwareX minimum reproducible revision

Manuscript 26 October 2026 cites 63b6f62. This copy starts from 198c800 and includes the existing uncommitted reviewer fixes. See the adjacent softwarex-assets-20261104 manifest for the source inventory and hashes.

| Manuscript evidence | Inputs and execution | Data and verification |
|---|---|---|
| Table 1, plugin contract | examples/jc_material_point.json → material adapter → Fortran bridge → JC | CTest interface_contract and jc_vumat_material_point |
| Table 2, material properties | configs/johnson_cook.json; same compiled plugin in both hosts | JC thermal-unit test documents the archived convention |
| Table 3 and Figures 4–5 | benchmarks/tensile3d/run_cases.py → PD case JSON or independent H8 executable | raw archives, analyse_and_plot.py, direct pair baseline and verify_metrics.py |
| Table 4 and Figure 6 | PD grid, horizon and time-step cases; finest H8 reference and symmetry check | raw archives and separate finest-reference metrics |
| Supplementary Movie S1 | coarse FEM optional snapshot output | original MP4, preview and provenance retained in benchmark media |
| Reviewer time-step follow-up | time_step_study.py and coarse PD input | validation/reviewer8-20261103 contains prior new-run evidence; the original fine-grid paper sequence is separate |

The archive contains the entire demos tree, presentation copies in docs/paper-figures, the Neo-Hookean material, its dedicated driver and test, and the two gallery-only tensile inputs. They are excluded from this candidate. The original copy and its unpublished changes remain recoverable. The bundled elastic plugin and two affine cases remain as regression fixtures. Optional outer damage stays because the Impact section describes it, and the benchmark explicitly disables it.

In the revised Abstract and Conclusions, replace references to runnable particle demos with the JC material-point quick start and paper reproduction guide. In Data and software availability, distinguish the historical demo snapshot from the future released minimal revision. Update its version link after approval and publication. Movie S1 stays available in the candidate. Zenodo publication is a separate step.

## Draft response to Reviewer 8

We streamlined the repository around two entry points: a Johnson–Cook material-point example and reproduction of the three-dimensional tensile benchmark. We removed the separate particle-gallery workflow and its dedicated large-motion application from the revision candidate. The benchmark retains the shared Johnson–Cook plugin, the independent H8 FEM program, the original case inputs, archived outputs and metric checks. The architecture guide now follows a material update from the host configuration and nonlocal kinematics through the C++/Fortran interface to stress return and force assembly. The interface documentation specifies tensor ordering, frames, units and history ownership. The contribution is the inspectable standalone plugin interface and its shared-material verification workflow. Users can compile the software, run the small example, and regenerate the published comparison metrics from the checked archive.

## Verification

Clean Windows GNU build, 15/15 default CTests, JC quick start, 41 archive hashes and 11 archived case metrics passed. Matched-pair and symmetry metrics passed their separate checks. Fresh coarse PD and H8 FEM both completed. CSV comparisons are in coarse-comparison.json. The full publication matrix was checked from archives. The fine-grid and finest H8 solves were not rerun.
