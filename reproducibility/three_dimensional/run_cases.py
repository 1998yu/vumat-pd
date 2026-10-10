"""Run the frozen 3D case matrix using a local CMake build."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import json
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[1]
FEM = {'fem_coarse': (2, 25), 'fem_fine': (1, 25),
       'fem_reference': (2 / 3, 25), 'fem_timecheck': (1, 12.5),
       'fem_symmetry_check': (2, 25)}
PD = ['pd_coarse', 'pd_shared', 'pd_m1p5', 'pd_delta4',
      'pd_delta2', 'pd_dt50', 'pd_dt12p5']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, default=REPO / 'build/release/bin')
    parser.add_argument('--cases', nargs='+', choices=list(FEM) + PD)
    parser.add_argument('--workers', type=int, default=1)
    parser.add_argument('--threads', type=int, default=1,
                        help='Only use multiple threads with audited reentrant VUMATs.')
    parser.add_argument('--overwrite', action='store_true',
                        help='Rerun selected cases in generated/results.')
    args = parser.parse_args()
    if args.threads < 1 or args.workers < 1:
        parser.error('workers and threads must be positive')
    bin_dir = args.bin_dir.resolve()
    generated = ROOT / 'generated'
    results = generated / 'results'
    logs = generated / 'logs'
    inputs = generated / 'inputs'
    for directory in (results, logs, inputs):
        directory.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    config = REPO / 'reproducibility/configs/johnson_cook.json'

    def run(name):
        dest = results / name
        if (dest / 'metadata.json').exists() and not args.overwrite:
            metadata = json.loads((dest / 'metadata.json').read_text())
            if metadata.get('finite') is True or metadata.get('status') == 'pass':
                print(f'EXISTING {name}', flush=True)
                return 0
            raise RuntimeError(f'{name} contains an unsuccessful prior run')
        if name in FEM:
            h, ns = FEM[name]
            command = [str(bin_dir / ('jc_3d_fem' + suffix)), str(config),
                       str(bin_dir), str(h), str(ns * 1e-6),
                       str(round(.12 / (ns * 1e-6))), str(dest)]
        else:
            case = json.loads((ROOT / 'cases' / f'{name}.json').read_text())
            case['output']['directory'] = dest.as_posix()
            case['materials'][0]['materialConfigPath'] = config.as_posix()
            case_path = inputs / f'{name}.json'
            case_path.write_text(json.dumps(case, indent=2))
            command = [str(bin_dir / ('vumat_pd_run' + suffix)), str(case_path)]
        env = dict(os.environ)
        env.update(OMP_NUM_THREADS=str(args.threads), OMP_DYNAMIC='FALSE')
        env.pop('FEM_SYMMETRY', None)
        if name in ['fem_reference', 'fem_symmetry_check']:
            env['FEM_SYMMETRY'] = '1'
        env['PATH'] = str(bin_dir) + os.pathsep + env.get('PATH', '')
        start = time.monotonic()
        print(f'START {name}', flush=True)
        # Remove only the completion marker, so an interrupted rerun cannot
        # accidentally be accepted as a previously successful simulation.
        (dest / 'metadata.json').unlink(missing_ok=True)
        with (logs / f'{name}.log').open('w') as log:
            result = subprocess.run(command, cwd=REPO, env=env,
                                    stdout=log, stderr=subprocess.STDOUT)
        (logs / f'{name}.execution.json').write_text(json.dumps({
            'case': name, 'returncode': result.returncode,
            'elapsed_seconds': time.monotonic() - start,
            'threads': args.threads, 'command': command}, indent=2))
        print(f'END {name}: {result.returncode}', flush=True)
        return result.returncode

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        codes = list(pool.map(run, args.cases or list(FEM) + PD))
    if any(codes):
        raise SystemExit('One or more cases failed; inspect generated/logs.')


if __name__ == '__main__':
    main()
