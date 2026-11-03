"""Run a fixed-space PD time-step study in a NEW directory; never reuse archives.

This measures temporal sensitivity, not a spectral stability limit or validation
of physical heating. The material configuration is kept byte-identical.
"""
import argparse
import copy
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time

REPO = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def compare(coarse, fine, amplitude):
    # The writer also emits step 1. Compare only identical physical output times;
    # never compare by row number or interpolate a missing transient sample.
    matches = []
    for row in coarse["history"]:
        match = next((r for r in fine["history"]
                      if math.isclose(float(r["time_ms"]), float(row["time_ms"]),
                                      rel_tol=1e-10, abs_tol=1e-14)), None)
        if match is not None:
            matches.append((row, match))
    if len(matches) < 2:
        raise ValueError("Fewer than two common history times")
    peak = max(abs(float(r["average_axial_stress_MPa"])) for _, r in matches)
    if peak == 0:
        raise ValueError("Zero reference stress history")
    squared = [(float(a["average_axial_stress_MPa"]) -
                float(b["average_axial_stress_MPa"])) ** 2 for a, b in matches]
    left, right = coarse["fields"], fine["fields"]
    if left.keys() != right.keys():
        raise ValueError("Final field points differ")
    displacement_squared = 0.0
    for point, a in left.items():
        b = right[point]
        if any(float(a[k]) != float(b[k]) for k in ("x0_mm", "y0_mm", "z0_mm")):
            raise ValueError("Reference geometry changed during time refinement")
        displacement_squared += sum((float(a[k]) - float(b[k])) ** 2
                                    for k in ("ux_mm", "uy_mm", "uz_mm"))
    peak_a = max(left.values(), key=lambda r: float(r["eqps"]))
    peak_b = max(right.values(), key=lambda r: float(r["eqps"]))
    eqps_b = float(peak_b["eqps"])
    return {
        "coarser_dt_ms": coarse["dt_ms"], "finer_dt_ms": fine["dt_ms"],
        "common_history_times": len(matches),
        "stress_history_nrmse_percent": 100 * math.sqrt(sum(squared) / len(squared)) / peak,
        "displacement_rmse_percent_of_amplitude":
            100 * math.sqrt(displacement_squared / (3 * len(left))) / abs(amplitude),
        "max_eqps_relative_difference_percent":
            100 * abs(float(peak_a["eqps"]) - eqps_b) / eqps_b if eqps_b else None,
        "coarser_peak_eqps_reference_xyz_mm": [float(peak_a[k]) for k in ("x0_mm", "y0_mm", "z0_mm")],
        "finer_peak_eqps_reference_xyz_mm": [float(peak_b[k]) for k in ("x0_mm", "y0_mm", "z0_mm")],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path, default=REPO / "benchmarks/tensile3d/cases/pd_coarse.json")
    parser.add_argument("--bin-dir", type=Path, default=REPO / "build/release/bin")
    parser.add_argument("--output", type=Path, required=True, help="Must not already exist")
    parser.add_argument("--threads", type=int, default=1)
    args = parser.parse_args()
    case_path = args.case.resolve()
    case = json.loads(case_path.read_text())
    if args.threads < 1 or case["type"] != "vumat_pd_boundary_case" or len(case["materials"]) != 1:
        parser.error("Requires positive threads and a single-material regional PD case")
    steps = case["time_integration"]["steps"]
    every = case["dynamics"]["outputEvery"]
    dt = case["time_integration"]["dt"]
    if (not isinstance(steps, int) or steps <= 0 or steps % 2 or
            not isinstance(every, int) or every < 2 or every % 2 or
            not math.isfinite(dt) or dt <= 0 or steps < every):
        parser.error("Base steps and outputEvery must be positive even integers; dt must be positive")
    duration = steps * dt
    if not math.isclose(duration, case["time_integration"]["totalTime"], rel_tol=1e-12):
        parser.error("Base totalTime must equal steps * dt")
    amplitude = case["boundary_conditions"]["amplitude"]
    if not math.isfinite(amplitude) or amplitude == 0:
        parser.error("Nonzero finite displacement amplitude is required")
    material = (REPO / case["materials"][0]["materialConfigPath"]).resolve()
    binary_dir = args.bin_dir.resolve()
    runner = binary_dir / ("vumat_pd_run.exe" if os.name == "nt" else "vumat_pd_run")
    if not runner.is_file() or not material.is_file():
        parser.error("Build the runner and supply an existing material configuration first")
    # Exclusive directory creation avoids silently mixing versions or failed reruns.
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    (output / "base-case.json").write_bytes(case_path.read_bytes())
    (output / "material.json").write_bytes(material.read_bytes())
    source_files = [p for folder in ("src", "include", "materials")
                    for p in (REPO / folder).rglob("*") if p.is_file()]
    binaries = [runner] + list(binary_dir.glob("*vumat_plugin.*"))
    report = {"status": "running", "scope": __doc__.strip(),
              "base_case_sha256": digest(case_path), "material_sha256": digest(material),
              "source_sha256": {p.relative_to(REPO).as_posix(): digest(p) for p in source_files},
              "binary_sha256": {p.name: digest(p) for p in binaries}, "runs": [], "comparisons": []}
    entry = case["materials"][0]
    young, poisson = entry.get("elasticE", 0), entry.get("elasticNu", 0)
    density = case["dynamics"]["density"]
    if young > 0 and density > 0 and -1 < poisson < 0.5:
        wave_speed = math.sqrt(young * (1 - poisson) /
                               (density * (1 + poisson) * (1 - 2 * poisson)))
        screen_dt = case["geometry"]["spacing"] / wave_speed
        report["continuum_screen"] = {
            "wave_speed_mm_per_ms": wave_speed, "h_over_cp_ms": screen_dt,
            "base_dt_over_h_over_cp": dt / screen_dt,
            "scope": "Continuum screening estimate using configured E and nu; NOT a PD stability bound"}
    report_path = output / "study.json"

    def save():
        report_path.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")

    save()
    results = []
    try:
        for label, factor in (("dt_x2", 2), ("dt_x1", 1), ("dt_x0p5", 0.5)):
            selected = copy.deepcopy(case)
            integration = selected["time_integration"]
            integration.update(dt=dt * factor, steps=round(steps / factor), totalTime=duration)
            selected["name"] = case["name"] + "_" + label
            selected["dynamics"].update(outputEvery=round(every / factor),
                                         snapshotEvery=integration["steps"])
            selected["materials"][0]["materialConfigPath"] = material.as_posix()
            selected["output"]["directory"] = (output / label).as_posix()
            input_path = output / (label + ".json")
            input_path.write_text(json.dumps(selected, indent=2) + "\n")
            env = dict(os.environ, OMP_NUM_THREADS=str(args.threads), OMP_DYNAMIC="FALSE")
            env["PATH"] = str(binary_dir) + os.pathsep + env.get("PATH", "")
            print(f"START {label}: dt={integration['dt']} ms", flush=True)
            started = time.monotonic()
            with (output / (label + ".log")).open("w") as log:
                result = subprocess.run([str(runner), str(input_path)], cwd=REPO, env=env,
                                        stdout=log, stderr=subprocess.STDOUT)
            run = {"label": label, "dt_ms": integration["dt"], "steps": integration["steps"],
                   "returncode": result.returncode, "threads": args.threads,
                   "elapsed_seconds": time.monotonic() - started}
            report["runs"].append(run)
            save()
            if result.returncode:
                raise RuntimeError(f"{label} failed; inspect its log")
            dest = output / label
            metadata = json.loads((dest / "metadata.json").read_text())
            history = read_rows(dest / "timeseries.csv")
            rows = read_rows(dest / "field_snapshots.csv")
            if (metadata["status"] != "pass" or len(rows) != metadata["particles"] or
                    not math.isclose(float(history[-1]["time_ms"]), duration, rel_tol=1e-10) or
                    any(r["finite"] != "1" for r in history) or
                    any(not math.isfinite(float(r[k])) for r in rows
                        for k in ("ux_mm", "uy_mm", "uz_mm", "eqps", "axial_stress_MPa"))):
                raise RuntimeError(f"{label} did not produce complete finite results")
            run["diagnostics"] = metadata["materials"][0]
            run["result_sha256"] = {p.name: digest(p) for p in dest.iterdir() if p.is_file()}
            results.append({"dt_ms": integration["dt"], "history": history,
                            "fields": {r["particle"]: r for r in rows}})
            print(f"END {label}: passed in {run['elapsed_seconds']:.1f}s", flush=True)
        report["comparisons"] = [compare(results[i], results[i + 1], amplitude) for i in range(2)]
        # A changed executable/configuration invalidates the mixed-run comparison.
        if (digest(material) != report["material_sha256"] or
                any(digest(p) != report["binary_sha256"][p.name] for p in binaries)):
            raise RuntimeError("Material or executable changed during study")
        report["status"] = "completed"
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        save()
    print(json.dumps(report["comparisons"], indent=2))


if __name__ == "__main__":
    main()
