# Manuscript figures used by the project

These figures explain the software and its documented verification. Figures 1–3 retain the supplied manuscript diagrams; the coupling diagram was rendered from its original vector metafile for browser display. Figures 4–6 were regenerated from the corrected Rodrigues-update benchmark results. The [provenance record](provenance.json) identifies each figure and its checksum.

| Manuscript figure | Project asset | Purpose |
|---|---|---|
| 1 | [Coupling workflow](coupling-workflow.png) | PD kinematics, material interface and PD force recovery |
| 2 | [Benchmark setup](benchmark-setup.png) | Fixed 3D geometry and one-sided smooth loading |
| 3 | [Verification path](verification-path.png) | Shared JC plugin in two independent spatial formulations |
| 4 | [Coarse fields](coarse-fields.png) | Matched coarse-grid stress and plastic-strain comparison |
| 5 | [Fine fields](fine-fields.png) | Matched fine-grid stress and plastic-strain comparison |
| 6 | [Sensitivity](sensitivity.png) | Grid, horizon and time-step comparisons with a common numerical reference |

Figures 4 and 5 show reference-coordinate fields: actual PD z = 0 values and FEM values averaged over the two nearest Gauss planes, displayed in quadrature subcells. Both methods share a full-range color scale. Stress annotations are interior means; plastic-strain annotations are global maxima, which need not lie in the displayed layer.

The matched FEM–PD metrics and the sensitivity metrics use different baselines. At matched fine resolution, stress-history NRMSE is 0.55% and displacement RMSE / imposed displacement is 0.96%. Relative to the finest H8 FEM reference, the fine PD deviations are 0.483%, 0.538% and 8.186% for stress history, displacement and maximum plastic strain. The finest FEM is a numerical approximation.

These plots belong to verification. The four runnable [PD particle demos](../../demos/README.md) have their own folders and discrete-particle animations. FEM is not a demo solver.

See [the benchmark](../../benchmarks/tensile3d/README.md) for archived data, separate metric baselines and reproduction commands. The optional outer damage layer is disabled in this benchmark; these figures do not validate damage or fracture prediction.
