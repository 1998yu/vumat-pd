"""Compare recomputed metrics with the frozen 3D reference outputs."""
from pathlib import Path
import json
import math

ROOT = Path(__file__).resolve().parent
expected = json.loads((ROOT / 'expected/analysis.json').read_text())
actual = json.loads((ROOT / 'generated/analysis.json').read_text())
for case, metrics in expected['metrics'].items():
    for name, value in metrics.items():
        if not math.isclose(value, actual['metrics'][case][name],
                            rel_tol=0.005, abs_tol=1e-8):
            raise SystemExit(f'Metric mismatch: {case}/{name}')
pair_expected = json.loads((ROOT / 'expected/direct_pair_metrics.json').read_text())
pair_actual = json.loads((ROOT / 'generated/direct_pair_metrics.json').read_text())
for case, metrics in pair_expected.items():
    for name, value in metrics.items():
        if not math.isclose(value, pair_actual[case][name],
                            rel_tol=0.005, abs_tol=1e-8):
            raise SystemExit(f'Matched FEM-PD metric mismatch: {case}/{name}')
print('PASS: all 11 cases match the frozen metrics within 0.5% relative tolerance.')
print('PASS: matched FEM-PD comparisons also match their separate PD baseline.')
