"""Compare completed JC FEM and VUMAT-PD quasi-2D benchmark outputs."""

from __future__ import annotations

import csv
import json
import math
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "generated"
DEST = ROOT / "generated" / "jc_2d_crosssolver_comparison.json"
TABLE = ROOT / "generated" / "table3.csv"


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def curve_metrics(pd_rows: list[dict[str, str]], fem_rows: list[dict[str, str]],
                  pd_key: str, fem_key: str) -> dict[str, float]:
    pd_by_time = {round(float(row["time_ms"]), 12): float(row[pd_key]) for row in pd_rows}
    pairs = [(pd_by_time[round(float(row["time_ms"]), 12)], float(row[fem_key]))
             for row in fem_rows if round(float(row["time_ms"]), 12) in pd_by_time]
    if not pairs:
        raise RuntimeError("No common output times")
    errors = [fem - pd for pd, fem in pairs]
    scale = max(abs(pd) for pd, _ in pairs) or 1.0
    final_pd, final_fem = pairs[-1]
    return {
        "points": len(pairs),
        "normalizedRmse": math.sqrt(sum(v * v for v in errors) / len(errors)) / scale,
        "maxAbsDifference": max(abs(v) for v in errors),
        "finalPd": final_pd,
        "finalFem": final_fem,
        "finalRelativeDifference": abs(final_fem - final_pd) / max(abs(final_pd), 1e-15),
    }


def field_metrics(pd_path: Path, fem_path: Path) -> dict[str, float]:
    pd_all = rows(pd_path)
    last_step = max(int(row["step"]) for row in pd_all)
    accum: dict[tuple[float, float], list[tuple[float, float]]] = defaultdict(list)
    for row in pd_all:
        if int(row["step"]) == last_step:
            key = (round(float(row["x0_mm"]), 10), round(float(row["y0_mm"]), 10))
            accum[key].append((float(row["ux_mm"]), float(row["uy_mm"])))
    pd_field = {key: (sum(v[0] for v in values) / len(values),
                      sum(v[1] for v in values) / len(values))
                for key, values in accum.items()}
    differences = []
    fem_final = rows(fem_path)
    for row in fem_final:
        # PD centres the transverse coordinate on y=0, whereas the FEM mesh is
        # generated on 0 <= y <= 20 mm.  Align coordinate origins before the
        # pointwise displacement comparison.
        key = (round(float(row["x0_mm"]), 10), round(float(row["y0_mm"]) - 10.0, 10))
        if key in pd_field:
            pd_u = pd_field[key]
            differences.append((float(row["ux_mm"]) - pd_u[0],
                                float(row["uy_mm"]) - pd_u[1]))
    if not differences:
        raise RuntimeError("No matching FEM/PD field coordinates")
    magnitudes = [math.hypot(dx, dy) for dx, dy in differences]
    return {
        "matchedCoordinates": len(differences),
        "displacementRmseMm": math.sqrt(sum(v * v for v in magnitudes) / len(magnitudes)),
        "displacementRmseNormalizedBy2mm":
            math.sqrt(sum(v * v for v in magnitudes) / len(magnitudes)) / 2.0,
        "maxDisplacementDifferenceMm": max(magnitudes),
    }


def compare(level: str) -> dict[str, object]:
    pd_dir = OUTPUT / f"jc_2d_crosssolver_{level}"
    fem_dir = OUTPUT / f"jc_2d_fem_{level}"
    pd_curve = rows(pd_dir / "timeseries.csv")
    fem_curve = rows(fem_dir / "timeseries.csv")
    return {
        "averageAxialStress": curve_metrics(
            pd_curve, fem_curve, "average_axial_stress_MPa", "average_axial_stress_MPa"),
        "averageAxialStrain": curve_metrics(
            pd_curve, fem_curve, "average_axial_strain", "average_axial_strain"),
        "maximumEquivalentPlasticStrain": curve_metrics(
            pd_curve, fem_curve, "max_eqps", "max_eqps"),
        "finalDisplacementField": field_metrics(
            pd_dir / "field_snapshots.csv", fem_dir / "final_nodes.csv"),
    }


def main() -> None:
    result = {
        "benchmark": "JC quasi-2D FEM-PD cross-solver verification",
        "damage": "disabled in both solvers",
        "commonVumatPlugin": "jc_vumat_plugin",
        "coarse": compare("coarse"),
        "fine": compare("fine"),
    }
    DEST.parent.mkdir(parents=True, exist_ok=True)
    DEST.write_text(json.dumps(result, indent=2), encoding="utf-8")
    with TABLE.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["Resolution", "Metric", "PD", "FEM", "Difference"])
        for level in ("coarse", "fine"):
            item = result[level]
            for key, label in (("averageAxialStress", "Final mean axial stress (MPa)"),
                               ("averageAxialStrain", "Final mean axial strain"),
                               ("maximumEquivalentPlasticStrain", "Maximum equivalent plastic strain")):
                metric = item[key]
                writer.writerow([level.capitalize(), label, metric["finalPd"], metric["finalFem"],
                                 metric["finalRelativeDifference"]])
            writer.writerow([level.capitalize(), "Stress-history NRMSE", "", "",
                             item["averageAxialStress"]["normalizedRmse"]])
            writer.writerow([level.capitalize(), "Displacement RMSE / 2 mm", "", "",
                             item["finalDisplacementField"]["displacementRmseNormalizedBy2mm"]])
    print(DEST)
    print(TABLE)
    for level in ("coarse", "fine"):
        item = result[level]
        print(level,
              "stress_final_pct=%.3f" % (100 * item["averageAxialStress"]["finalRelativeDifference"]),
              "eqps_final_pct=%.3f" % (100 * item["maximumEquivalentPlasticStrain"]["finalRelativeDifference"]),
              "field_rmse_pct=%.3f" % (100 * item["finalDisplacementField"]["displacementRmseNormalizedBy2mm"]))


if __name__ == "__main__":
    main()
