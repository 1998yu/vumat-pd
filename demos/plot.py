"""Plot freshly solved PD elastic or Johnson-Cook tensile fields."""
from pathlib import Path
import argparse
import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from pd_results import load_case

REPO = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', choices=['elastic', 'tensile'], required=True)
    parser.add_argument('--output-dir', type=Path, default=REPO / 'demos/output')
    args = parser.parse_args()
    data = load_case(REPO / 'demos/output' / ('elastic_pd' if args.demo == 'elastic' else 'jc_pd'))
    plt.rcParams.update({'font.family': 'sans-serif', 'font.sans-serif': ['Arial', 'DejaVu Sans'],
                         'font.size': 12, 'svg.fonttype': 'none', 'axes.spines.top': False, 'axes.spines.right': False})
    fig, axes = plt.subplots(3, 1, figsize=(10, 8), layout='constrained', gridspec_kw={'height_ratios': [1, 1, 1.4]})
    variables = [('stress', 'Axial stress (MPa)', 'viridis')]
    variables += [('ux', 'Axial displacement (mm)', 'cividis')] if args.demo == 'elastic' else [('eqps', 'Equivalent plastic strain', 'magma')]
    for ax, (key, label, cmap) in zip(axes, variables):
        im = ax.pcolormesh(data['x_edges'], data['y_edges'], data[key][-1], cmap=cmap, shading='flat')
        ax.set(xlabel='Reference x (mm)', ylabel='y (mm)', title=label, xlim=(0, 100), ylim=(-10, 10))
        ax.set_aspect('equal')
        fig.colorbar(im, ax=ax, shrink=.8, label='MPa' if key == 'stress' else 'mm' if key == 'ux' else 'strain')
    hist = data['history']
    axes[2].plot(hist['time_ms'], hist['average_axial_stress_MPa'], color='#246b95', lw=2)
    axes[2].set(xlabel='Simulation time (ms)', ylabel='Mean axial stress (MPa)', title='Computed PD stress history', xlim=(0, .12))
    axes[2].grid(axis='y', color='#e3e8eb')
    name = 'Elastic' if args.demo == 'elastic' else 'Johnson–Cook'
    fig.suptitle(f'Correspondence PD + {name} | actual 3D tensile solution', fontsize=17, weight='bold')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output_dir / (args.demo + '.png'), dpi=180)
    fig.savefig(args.output_dir / (args.demo + '.svg'))
    (args.output_dir / (args.demo + '-plot.json')).write_text(json.dumps(data['proof'], indent=2) + '\n', encoding='utf-8')
    print('PD plot:', args.output_dir / (args.demo + '.png'))

if __name__ == '__main__':
    main()
