"""Generate the coarse and fine FEM-PD contour panels as dependency-free SVG."""

from __future__ import annotations

import csv
import html
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "generated"
DEST = OUTPUT / "figures"


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def final_pd(level: str) -> list[dict[str, str]]:
    data = rows(OUTPUT / f"jc_2d_crosssolver_{level}" / "field_snapshots.csv")
    final_step = max(int(row["step"]) for row in data)
    return [row for row in data
            if int(row["step"]) == final_step and abs(float(row["z0_mm"])) < 1e-12]


def fem(level: str) -> list[dict[str, str]]:
    return rows(OUTPUT / f"jc_2d_fem_{level}" / "final_gauss_points.csv")


def quantile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    index = min(len(ordered) - 1, round(fraction * (len(ordered) - 1)))
    return ordered[index]


def color(value: float, maximum: float, palette: str) -> str:
    t = max(0.0, min(1.0, value / maximum if maximum else 0.0))
    if palette == "stress":
        stops = ((42, 35, 92), (37, 130, 142), (94, 201, 98), (253, 231, 37))
    else:
        stops = ((0, 0, 4), (84, 15, 109), (187, 55, 84), (252, 253, 191))
    scaled = t * (len(stops) - 1)
    left = min(int(scaled), len(stops) - 2)
    local = scaled - left
    rgb = tuple(round(stops[left][i] * (1.0 - local) + stops[left + 1][i] * local)
                for i in range(3))
    return "#%02x%02x%02x" % rgb


def panel(svg: list[str], data: list[dict[str, str]], x_key: str, y_key: str,
          value_key: str, title: str, note: str, x0: int, y0: int,
          width: int, height: int, maximum: float, palette: str,
          y_shift: float = 0.0) -> None:
    svg.append(f'<rect x="{x0}" y="{y0}" width="{width}" height="{height}" fill="white" stroke="#8290a0"/>')
    svg.append(f'<text x="{x0}" y="{y0 - 10}" class="title">{html.escape(title)}</text>')
    marker = 2.4 if len(data) < 3000 else 1.5
    for row in data:
        x = float(row[x_key])
        y = float(row[y_key]) + y_shift
        value = float(row[value_key])
        px = x0 + width * x / 102.0
        py = y0 + height * (11.0 - y) / 22.0
        fill = color(value, maximum, palette)
        svg.append(f'<rect x="{px - marker/2:.2f}" y="{py - marker/2:.2f}" width="{marker:.2f}" height="{marker:.2f}" fill="{fill}"/>')
    svg.append(f'<text x="{x0 + width - 5}" y="{y0 + height - 7}" text-anchor="end" class="note">{html.escape(note)}</text>')
    svg.append(f'<text x="{x0 + width/2}" y="{y0 + height + 18}" text-anchor="middle" class="axis">x (mm)</text>')
    svg.append(f'<text x="{x0 - 25}" y="{y0 + height/2}" text-anchor="middle" class="axis" transform="rotate(-90 {x0 - 25} {y0 + height/2})">y (mm)</text>')


def final_metric(level: str, method: str, key: str) -> float:
    directory = f"jc_2d_crosssolver_{level}" if method == "pd" else f"jc_2d_fem_{level}"
    return float(rows(OUTPUT / directory / "timeseries.csv")[-1][key])


def make(level: str) -> None:
    pd_data = final_pd(level)
    fem_data = fem(level)
    stresses = ([float(row["axial_stress_MPa"]) for row in pd_data] +
                [float(row["s11_MPa"]) for row in fem_data])
    eqps = ([float(row["eqps"]) for row in pd_data] +
            [float(row["eqps"]) for row in fem_data])
    stress_max = quantile(stresses, 0.98)
    eqps_max = max(eqps)
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="650" viewBox="0 0 1200 650">',
        '<style>text{font-family:Arial,Helvetica,sans-serif;fill:#1e2933}.title{font-size:16px;font-weight:bold}.note{font-size:12px}.axis{font-size:12px}</style>',
        '<rect width="1200" height="650" fill="white"/>',
    ]
    w, h = 500, 215
    panel(svg, pd_data, "x_mm", "y_mm", "axial_stress_MPa",
          "(a) PD: axial stress, S11",
          f"interior mean = {final_metric(level, 'pd', 'average_axial_stress_MPa'):.1f} MPa",
          70, 65, w, h, stress_max, "stress")
    panel(svg, fem_data, "x_mm", "y_mm", "s11_MPa",
          "(b) FEM: axial stress, S11",
          f"interior mean = {final_metric(level, 'fem', 'average_axial_stress_MPa'):.1f} MPa",
          650, 65, w, h, stress_max, "stress", -10.0)
    panel(svg, pd_data, "x_mm", "y_mm", "eqps",
          "(c) PD: equivalent plastic strain", f"max = {max(float(r['eqps']) for r in pd_data):.5f}",
          70, 380, w, h, eqps_max, "eqps")
    panel(svg, fem_data, "x_mm", "y_mm", "eqps",
          "(d) FEM: equivalent plastic strain", f"max = {max(float(r['eqps']) for r in fem_data):.5f}",
          650, 380, w, h, eqps_max, "eqps", -10.0)
    svg.append('</svg>')
    DEST.mkdir(parents=True, exist_ok=True)
    (DEST / f"jc_2d_crosssolver_{level}_contours.svg").write_text("\n".join(svg), encoding="utf-8")


if __name__ == "__main__":
    for resolution in ("coarse", "fine"):
        make(resolution)
    print(DEST)
