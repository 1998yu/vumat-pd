"""Check regenerated benchmark metrics against the archived expected values."""

from __future__ import annotations

import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ACTUAL = ROOT / "generated" / "jc_2d_crosssolver_comparison.json"
EXPECTED = ROOT / "expected" / "jc_2d_crosssolver_comparison.json"
RELATIVE_TOLERANCE = 5.0e-3
ABSOLUTE_TOLERANCE = 1.0e-10


def numeric_leaves(value, path=()):
    if isinstance(value, dict):
        for key, child in value.items():
            yield from numeric_leaves(child, path + (key,))
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        yield path, float(value)


def main() -> None:
    actual = json.loads(ACTUAL.read_text(encoding="utf-8"))
    expected = json.loads(EXPECTED.read_text(encoding="utf-8"))
    actual_values = dict(numeric_leaves(actual))
    failures = []
    for path, reference in numeric_leaves(expected):
        value = actual_values.get(path)
        if value is None or not math.isclose(value, reference,
                                              rel_tol=RELATIVE_TOLERANCE,
                                              abs_tol=ABSOLUTE_TOLERANCE):
            failures.append((".".join(path), reference, value))
    if failures:
        for path, reference, value in failures:
            print(f"FAIL {path}: expected {reference!r}, got {value!r}")
        raise SystemExit(1)
    print(f"PASS: all archived metrics reproduced within {RELATIVE_TOLERANCE:.2%} relative tolerance")


if __name__ == "__main__":
    main()
