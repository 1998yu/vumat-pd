"""Read actual time-resolved PD results, rejecting other solver outputs."""
from pathlib import Path
import hashlib
import json
import numpy as np

def load_case(root):
    root = Path(root)
    meta = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
    if meta.get('solver') != 'correspondence peridynamics':
        raise ValueError('This reader accepts correspondence-PD solver outputs only.')
    assert meta['status'] == 'pass'
    assert len(meta['materials']) == 1 and meta['materials'][0]['finite']
    snapshot = root / 'field_snapshots.csv'
    raw = np.loadtxt(snapshot, delimiter=',', skiprows=1,
                     usecols=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 17, 18])
    assert np.isfinite(raw).all()
    fields = raw.reshape(-1, meta['particles'], 14)
    assert np.all(fields[:, :, 0] == fields[:, :1, 0])
    assert np.all(fields[:, :, 1] == fields[:, :1, 1])
    assert np.array_equal(fields[:, :, 2], np.broadcast_to(fields[0, :, 2], fields[:, :, 2].shape))
    assert np.allclose(fields[:, :, 3:6], fields[0, :, 3:6], atol=1e-10, rtol=0)
    assert np.allclose(fields[:, :, 9:12], fields[:, :, 3:6] + fields[:, :, 6:9], atol=1e-9, rtol=0)
    times = fields[:, 0, 1]
    assert np.all(np.diff(times) > 0) and abs(times[-1] - .12) < 1e-10
    history = np.genfromtxt(root / 'timeseries.csv', delimiter=',', names=True, dtype=None, encoding='utf-8')
    assert np.all(history['finite'] == 1)
    for key in history.dtype.names:
        if history.dtype[key].kind in 'fi':
            assert np.isfinite(history[key]).all(), key
    proof = {'solver': meta['solver'], 'material': meta['materials'][0]['name'],
             'plugin_file': meta['materials'][0]['plugin_file'], 'particles': meta['particles'],
             'actual_states': len(times), 'simulation_time_ms': [float(times[0]), float(times[-1])],
             'field_sampling': 'all actual PD particles at saved deformed 3D coordinates',
             'snapshots_sha256': hashlib.sha256(snapshot.read_bytes()).hexdigest(),
             'history_sha256': hashlib.sha256((root / 'timeseries.csv').read_bytes()).hexdigest()}
    return {'meta': meta, 'history': history, 'times': times,
            'reference_particles': fields[0, :, 3:6],
            'particle_positions': fields[:, :, 9:12],
            'particle_stress': fields[:, :, 12],
            'particle_eqps': fields[:, :, 13], 'proof': proof}
