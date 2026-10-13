"""Run actual correspondence-PD demos with their compiled material plugins."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('demo', choices=['elastic', 'tensile', 'transient', 'hyperelastic', 'all'])
    p.add_argument('--bin-dir', type=Path, default=REPO / 'build/release/bin')
    p.add_argument('--threads', type=int, default=1)
    a = p.parse_args()
    if a.threads < 1:
        p.error('threads must be positive')
    bin_dir = a.bin_dir.resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    runner = bin_dir / ('vumat_pd_run' + suffix)
    motion = bin_dir / ('vumat_pd_large_motion_3d' + suffix)
    if not runner.exists():
        p.error('Build with cmake --preset release and cmake --build --preset release first.')
    env = dict(os.environ, OMP_NUM_THREADS=str(a.threads), OMP_DYNAMIC='FALSE')
    env['PATH'] = str(bin_dir) + os.pathsep + env.get('PATH', '')
    entries = []

    def run(cmd):
        subprocess.run([str(value) for value in cmd], cwd=REPO, env=env, check=True)

    def record(name, exe, config, result):
        meta = json.loads((result / 'metadata.json').read_text(encoding='utf-8'))
        assert meta['solver'] == 'correspondence peridynamics'
        assert meta['status'] in ['pass', 'completed']
        plugin = json.loads(config.read_text(encoding='utf-8'))['plugin']
        libraries = [bin_dir / (plugin + ext) for ext in ['.dll', '.so', '.dylib']]
        library = next((path for path in libraries if path.exists()), None)
        assert library is not None, f'Missing plugin {plugin}'
        if 'materials' in meta:
            assert meta['materials'][0]['plugin_file'] == library.name
        else:
            assert meta['plugin_file'] == plugin
        entries.append({'case': name, 'solver': meta['solver'], 'executable': exe.name,
                        'material_config': config.relative_to(REPO).as_posix(), 'plugin': library.name,
                        'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
                        'plugin_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
                        'result_directory': result.relative_to(REPO).as_posix(),
                        'snapshots_sha256': hashlib.sha256((result / 'field_snapshots.csv').read_bytes()).hexdigest(),
                        'history_sha256': hashlib.sha256((result / 'timeseries.csv').read_bytes()).hexdigest(),
                        'status': meta['status']})

    if a.demo in ['elastic', 'all']:
        run([runner, REPO / 'examples/elastic_tension_pd.json'])
        record('elastic_tension', runner, REPO / 'configs/elastic.json', REPO / 'demos/output/elastic_pd')
    if a.demo in ['tensile', 'transient', 'all']:
        run([runner, REPO / 'examples/jc_tension_pd.json'])
        record('johnson_cook_tension', runner, REPO / 'configs/johnson_cook.json', REPO / 'demos/output/jc_pd')
    if a.demo in ['hyperelastic', 'all']:
        if not motion.exists():
            p.error('Rebuild to include the hyperelastic PD entry point.')
        for name, angle in [('bending', 150), ('torsion', 540)]:
            result = REPO / 'demos/output/hyperelastic' / name
            run([motion, name, result, 2400, 24, angle])
            record(name, motion, REPO / 'configs/neohookean.json', result)
    out = REPO / 'demos/output'
    out.mkdir(exist_ok=True)
    (out / 'run_manifest.json').write_text(json.dumps({'all_displayed_simulations_use_PD': True,
        'threads': a.threads, 'cases': entries}, indent=2) + '\n', encoding='utf-8')
    if a.demo == 'transient':
        run([sys.executable, REPO / 'demos/render_particles.py', '--case', 'jc'])
    print('PD runs completed. Results and executable/plugin provenance: demos/output/run_manifest.json')

if __name__ == '__main__':
    main()
