import gzip
import json
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parent
archive = root.parent / "raw"
names = ("pd_coarse", "fem_coarse", "pd_shared", "fem_fine")
times = np.arange(1, 241) * .0005
coords = np.array([(x,y,z) for x in np.arange(4,98,2) for y in np.arange(-10,12,2) for z in [-2,0,2]])
columns = ("average_axial_stress_MPa", "max_eqps")

def read(name, filename, legacy):
    path = archive/name/(filename+".gz") if legacy else root/"raw"/name/(filename+".gz")
    opener = gzip.open
    with opener(path, "rt") as file:
        return np.genfromtxt(file, delimiter=",", names=True)

def displacement(nodes):
    axes = [np.unique(nodes[k]) for k in ("x0_mm","y0_mm","z0_mm")]
    order = np.lexsort((nodes["z0_mm"],nodes["y0_mm"],nodes["x0_mm"]))
    grid = np.column_stack([nodes[k][order] for k in ("ux_mm","uy_mm","uz_mm")]).reshape(*(len(a) for a in axes),3)
    indices = [np.clip(np.searchsorted(axis, coords[:,i], side="right")-1, 0, len(axis)-2) for i,axis in enumerate(axes)]
    fractions = [(coords[:,i]-axis[j])/(axis[j+1]-axis[j]) for i,(axis,j) in enumerate(zip(axes,indices))]
    result = np.zeros((len(coords),3))
    for corner in np.ndindex(2,2,2):
        weight = np.prod([f if bit else 1-f for f,bit in zip(fractions,corner)], axis=0)
        result += weight[:,None]*grid[tuple(j+bit for j,bit in zip(indices,corner))]
    return result

def load(name, legacy):
    series = read(name,"timeseries.csv",legacy)
    assert np.all(series["finite"] == 1) and np.isclose(series["time_ms"][-1],.12)
    field = read(name,"field_snapshots.csv" if name.startswith("pd") else "final_gauss_points.csv",legacy)
    nodes = field if name.startswith("pd") else read(name,"final_nodes.csv",legacy)
    temp = next((k for k in field.dtype.names if "temp" in k.lower()),None)
    return dict(final={k:float(series[k][-1]) for k in columns},
                stress=np.interp(times,series["time_ms"],series[columns[0]]),
                displacement=displacement(nodes),
                max_temperature_K=float(np.max(field[temp])) if temp else None)

def differences(base, new):
    result = {k:float(100*(new["final"][k]-base["final"][k])/abs(base["final"][k])) for k in columns}
    result["stress_history_nrmse_pct"] = float(100*np.sqrt(np.mean((new["stress"]-base["stress"])**2))/np.max(abs(base["stress"])))
    result["displacement_rmse_pct_of_2mm"] = float(50*np.sqrt(np.mean(np.sum((new["displacement"]-base["displacement"])**2,axis=1))))
    return result

summary = {}
pairs = {}
for name in names:
    record = root / f"{name}.execution.json"
    if not record.exists() or json.loads(record.read_text())["returncode"] != 0:
        raise RuntimeError(f"Missing or unsuccessful execution record: {name}")
    old, new = load(name,True), load(name,False)
    summary[name] = dict(legacy=old["final"], corrected=new["final"], legacy_max_temperature_K=old["max_temperature_K"],
                         corrected_max_temperature_K=new["max_temperature_K"], differences_pct=differences(old,new))
    pairs[name] = old,new
for label, pd, fem in [("coarse","pd_coarse","fem_coarse"),("fine","pd_shared","fem_fine")]:
    if pd in pairs and fem in pairs:
        summary[f"{label}_fem_vs_pd"] = {tag:differences(pairs[pd][i],pairs[fem][i]) for i,tag in enumerate(("legacy","corrected"))}
(root/"comparison.json").write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
