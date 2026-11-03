"""Exercise public JSON entry points rather than only testing configuration structs."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

runner = Path(sys.argv[1]).resolve()
base = json.loads(Path("examples/jc_material_point.json").read_text())
cases = [
    ({"kinematicsMode": "legacy_small_strain"}, "Unsupported kinematicsMode"),

    ({"internalIndicator": {"indices": [1], "reduction": "sum"}}, "reduction must be"),
    ({"internalIndicator": {"indices": [1], "transform": "unknown"}}, "transform must be"),
    ({"initialState": [0.0], "initialStateOverrides": {"5": 1.0}}, "initialState length"),
    ({"equivalentPlasticStrainIndex": -2}, "outside nstatev"),
    ({"internalIndicator": {"indices": [1.5]}}, "nonnegative integer"),
    ({"internalIndicator": {"indices": ["1"]}}, "nonnegative integer"),
    ({"initialStateOverrides": {"1junk": 1.0}}, "Invalid initialStateOverrides index"),
    ({"initialState": [0.0, "bad", 0.0, 0.0, 0.0, 0.0, 0.0]}, "Expected numeric entries"),
    ({"initialState": "not an array"}, "Expected numeric array"),
    ({"internalIndicator": {"indices": "1"}}, "Expected integer array"),
]
with tempfile.TemporaryDirectory() as directory:
    for index, (changes, message) in enumerate(cases):
        case = copy.deepcopy(base)
        case["materials"][0].update(changes)
        case["output"]["directory"] = str(Path(directory) / "must_not_run")
        path = Path(directory) / f"invalid_{index}.json"
        path.write_text(json.dumps(case))
        result = subprocess.run([str(runner), str(path)], capture_output=True, text=True)
        if result.returncode != 1 or message not in result.stderr:
            raise RuntimeError(f"Rejection failed: {message}: {result.returncode} {result.stderr}")
        assert not Path(case["output"]["directory"]).exists()
    for index, (field, value, message) in enumerate([
        ("steps", 1.5, "representable integer"),
        ("steps", 2147483648, "representable integer"),
        ("dt", 0.0, "dt > 0"),
        ("dt", -1.0, "dt > 0"),
        ("dt", 1.0e308, "duration must be finite"),
    ]):
        case = copy.deepcopy(base)
        case["time_integration"][field] = value
        case["output"]["directory"] = str(Path(directory) / "must_not_run")
        path = Path(directory) / f"invalid_time_{index}.json"
        path.write_text(json.dumps(case))
        result = subprocess.run([str(runner), str(path)], capture_output=True, text=True)
        if result.returncode != 1 or message not in result.stderr:
            raise RuntimeError(f"Time rejection failed: {message}: {result.returncode} {result.stderr}")
        assert not Path(case["output"]["directory"]).exists()
print("PASS unsupported/material-index/time configurations rejected before execution")
