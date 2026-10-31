"""Isolated work-to-heat conversion audit; never changes frozen benchmark data."""
from pathlib import Path
import argparse
import csv
import gzip
import io
import json
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin-dir", type=Path, default=REPO/"build/release/bin")
    args = parser.parse_args()
    binary = args.bin_dir.resolve()
    out = ROOT/"generated/thermal_audit"
    out.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    env = dict(os.environ, OMP_NUM_THREADS="2", OMP_DYNAMIC="FALSE")
    env.pop("FEM_SYMMETRY", None)
    env.pop("FEM_SNAPSHOT_EVERY", None)
    commands = []
    material = REPO/"configs/johnson_cook_heat_units.json"
    for label in ("legacy_point", "heat_units_point"):
        case = json.loads((REPO/"examples/jc_material_point.json").read_text())
        if label == "heat_units_point":
            case["materials"][0]["materialConfigPath"] = material.as_posix()
        case["output"]["directory"] = (out/label).as_posix()
        path = out/f"{label}.json"
        path.write_text(json.dumps(case, indent=2))
        commands.append((label, [str(binary/("vumat_pd_run"+suffix)), str(path)]))
    pd = json.loads((ROOT/"cases/pd_coarse.json").read_text())
    pd["materials"][0]["materialConfigPath"] = material.as_posix()
    pd["output"]["directory"] = (out/"pd_heat_units").as_posix()
    path = out/"pd_heat_units.json"
    path.write_text(json.dumps(pd, indent=2))
    commands.append(("pd_heat_units", [str(binary/("vumat_pd_run"+suffix)), str(path)]))
    commands.append(("fem_heat_units", [str(binary/("jc_3d_fem"+suffix)), str(material),
                     str(binary), "2", "0.000025", "4800", str(out/"fem_heat_units")]))
    execution = []
    for label, command in commands:
        started = time.monotonic()
        with (out/f"{label}.log").open("w") as log:
            result = subprocess.run(command, cwd=REPO, env=env, stdout=log, stderr=subprocess.STDOUT)
        execution.append({"case": label, "returncode": result.returncode,
                          "elapsed_seconds": time.monotonic()-started,
                          "command": [v.replace(str(REPO), "<CHECKOUT>").replace(REPO.as_posix(), "<CHECKOUT>") for v in command]})
        (out/"execution.json").write_text(json.dumps(execution, indent=2))
        print(label, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(f"{label} failed; inspect its retained log")
    def last(path, compressed=False):
        text = gzip.decompress(path.read_bytes()).decode() if compressed else path.read_text()
        return list(csv.DictReader(io.StringIO(text)))[-1]
    summary = {"scope": "isolated heat-unit sensitivity; not physical thermomechanical validation",
               "spatial_settings": {"h_mm": 2, "delta_mm": 6, "dt_ns": 25, "duration_ms": 0.12},
               "material_point": {}, "coarse_comparisons": {}}
    for label in ("legacy_point", "heat_units_point"):
        row = last(out/label/"timeseries.csv")
        summary["material_point"][label] = {k: float(row[k]) for k in ("stress","temperature","state_indicator")}
    for solver in ("pd", "fem"):
        old = last(ROOT/f"raw/{solver}_coarse/timeseries.csv.gz", True)
        new = last(out/f"{solver}_heat_units/timeseries.csv")
        summary["coarse_comparisons"][solver] = {
            k: {"legacy": float(old[k]), "heat_units": float(new[k]),
                "change_pct_of_legacy": 100*(float(new[k])-float(old[k]))/abs(float(old[k]))}
            for k in ("average_axial_stress_MPa", "max_eqps")}
    (out/"summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))

if __name__ == "__main__":
    main()
