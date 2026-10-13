# Optional outer damage layer

`SimpleDamageModel` and `PdSimpleDamage` provide optional host-level damage operations separately from a material plugin. They are part of the PD library and are called by the corotational spatial/affine hosts. The dedicated Neo-Hookean bending/torsion host uses intact bonds and does not enable this layer.

## Default behavior

Every supplied PD demo and paper benchmark explicitly disables outer damage:

```json
"damage": {
  "model": "none",
  "initiation": 0,
  "failure": 1,
  "deletionEnabled": false
}
```

Disabled operation passes the raw material stress through unchanged and leaves bonds intact. Material state and JC plasticity continue to evolve through the VUMAT.

## Available operations

| Model | Driver | Host effect |
|---|---|---|
| `none` | None | Preserve stress and intact bonds |
| `eqps_threshold` | Material state at the configured C++ state index; default index 1 for the supplied JC routine | Irreversible linear degradation between initiation and failure |
| `max_principal_strain` | Maximum principal value of the symmetric strain tensor | Irreversible linear stress degradation between initiation and failure |
| `critical_stretch` | Change in bond length relative to its reference length | Irreversible directed-bond failure beyond the tensile threshold, with optional compression threshold |

Point modes return an effective stress for the host's force assembly while leaving the constitutive raw-stress/history arrays intact. The critical-stretch mode attenuates bond force contributions and reports a neighborhood-volume-weighted damage index; its returned stress is unchanged.

For point modes, `failure` must exceed `initiation`. For critical stretch, `initiation` is a positive tensile threshold; positive finite `failure` also sets the compression threshold. Set `failure` to zero for tension-only bond failure.

`deletionEnabled` sets the failed-point flag and effective-stress behavior. It does not establish a validated strategy for deleting particles, rebuilding shape tensors or handling fracture surfaces. The current kinematic reconstruction uses the original neighborhoods.

## Verification scope

The outer-damage test checks disabled behavior, material-state separation, point-threshold degradation, principal-strain evaluation, irreversibility, invalid thresholds and tensile/compressive bond failure. The paper's JC comparisons and the four particle-demo regressions use `none`; their agreement does not validate damage or fracture prediction.

Run the algorithmic check after building:

```bash
ctest --preset release -R outer_damage
```

Before applying these operations to a physical damage/fracture case, verify its constitutive and coupling behavior independently. See [the interface guide](interface.md) for material/frame conventions.
