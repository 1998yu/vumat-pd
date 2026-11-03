# Execution path
The regional case runner reads the JSON configuration and constructs reference nodes, volumes and neighborhoods in src/host. The PD kernel reconstructs the deformation gradient from reference and current bonds. The corotational update supplies the strain and rotation measures used by the material adapter.

VumatPdCorrespondenceMaterial owns per-point material history. VumatMaterialAdapter packs material points into blocks and calls the Fortran bridge and Johnson-Cook plugin. See [interface conventions](interface.md) for component ordering, units and stress frames. The PD host rotates stress back to the spatial frame and assembles reference force density. The integrator divides force density by reference density to obtain acceleration. Integrating force density over reference volumes gives the net internal force.

The independent H8 solver in benchmarks/tensile3d/src shares the material adapter and plugin. Its element integration and dynamics provide a separate numerical reference.
