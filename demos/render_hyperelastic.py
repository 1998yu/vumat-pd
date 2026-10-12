"""Render actual Neo-Hookean bending and torsion snapshots side by side."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize

REPO = Path(__file__).resolve().parents[1]

def read_case(root):
    meta = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
    history = np.genfromtxt(root / 'timeseries.csv', delimiter=',', names=True)
    raw = np.genfromtxt(root / 'field_snapshots.csv', delimiter=',', names=True)
    for data in [history, raw]:
        for name in data.dtype.names:
            assert np.isfinite(data[name]).all(), f'Nonfinite {name}'
    assert meta['status'] == 'completed' and np.all(history['finite'] == 1)
    assert np.all(history['min_J'] > 0)
    steps = np.unique(raw['step']).astype(int)
    fields = [raw[raw['step'] == step] for step in steps]
    assert all(len(frame) == meta['particles'] for frame in fields)
    assert all(np.array_equal(frame['particle'], fields[0]['particle']) for frame in fields)
    rows = [history[history['step'] == step][0] for step in steps]
    assert all(np.allclose(frame['time_ms'], row['time_ms']) for frame, row in zip(fields, rows))
    return {'root': root, 'meta': meta, 'history': history, 'steps': steps,
            'fields': fields, 'rows': rows}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, default=REPO / 'demos/output/hyperelastic')
    parser.add_argument('--output', type=Path, default=REPO / 'demos/output/hyperelastic.mp4')
    args = parser.parse_args()
    ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
    if not ffmpeg or not ffprobe:
        parser.error('Install FFmpeg and put ffmpeg and ffprobe on PATH.')
    cases = [read_case(args.results / name) for name in ['bending', 'torsion']]
    assert np.array_equal(cases[0]['steps'], cases[1]['steps'])
    assert np.isclose(cases[0]['rows'][-1]['angle_deg'], 150)
    assert np.isclose(cases[1]['rows'][-1]['angle_deg'], 540)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    vmax = max(float(frame['von_mises_MPa'].max()) for case in cases for frame in case['fields'])
    norm = Normalize(0, vmax)
    plt.rcParams.update({'font.family': 'sans-serif', 'font.sans-serif': ['Arial', 'DejaVu Sans'],
                         'font.size': 11, 'axes.spines.top': False, 'axes.spines.right': False})
    fig = plt.figure(figsize=(14, 8.3), dpi=120, facecolor='white')
    fig.text(.04, .953, 'Neo-Hookean | large deformation', fontsize=24, weight='bold', color='#182630')
    fig.text(.04, .915, 'PD force balance · fixed base and prescribed end rotation · free interior particles', fontsize=13, color='#435966')
    timer = fig.text(.79, .953, '', fontsize=15, weight='bold', color='#182630')
    dots, outlines, labels, markers = [], [], [], []
    for col, case in enumerate(cases):
        left = .035 + col * .49
        ax = fig.add_axes([left, .385, .46, .47], projection='3d')
        positions = np.concatenate([np.column_stack([frame[k] for k in ['x_mm', 'y_mm', 'z_mm']]) for frame in case['fields']])
        lo, hi = positions.min(axis=0), positions.max(axis=0)
        pad = np.maximum((hi - lo) * .10, 2)
        lo, hi = lo - pad, hi + pad
        for setter, lower, upper in zip([ax.set_xlim, ax.set_ylim, ax.set_zlim], lo, hi):
            setter(lower, upper)
        ax.set_box_aspect(hi - lo)
        ax.view_init(elev=25, azim=-62)
        ax.set(xlabel='x (mm)', ylabel='y (mm)', zlabel='z (mm)')
        ax.set_zticks([-4, 0, 4])
        if case['meta']['scenario'] == 'torsion':
            ax.set_yticks([-4, 0, 4])
        ax.tick_params(labelsize=8)
        for axis in [ax.xaxis, ax.yaxis, ax.zaxis]:
            axis.pane.set_facecolor((.97, .98, .99, 1))
            axis._axinfo['grid']['color'] = (.7, .75, .8, .25)
        frame = case['fields'][0]
        dot = ax.scatter(frame['x_mm'], frame['y_mm'], frame['z_mm'], c=frame['von_mises_MPa'],
                         s=8, cmap='viridis', norm=norm, depthshade=False, edgecolors='none')
        dots.append(dot)
        edges = []
        for y in [-4, 4]:
            for z in [-4, 4]:
                mask = np.isclose(frame['y0_mm'], y) & np.isclose(frame['z0_mm'], z)
                ids = np.flatnonzero(mask)
                ids = ids[np.argsort(frame['x0_mm'][ids])]
                line, = ax.plot(frame['x_mm'][ids], frame['y_mm'][ids], frame['z_mm'][ids], color='#273c48', lw=.7)
                edges.append((line, ids))
        outlines.append(edges)
        fig.text(left + .02, .866, f'{case["meta"]["scenario"].capitalize()} | {case["meta"]["maximumAngle_deg"]}° end rotation', fontsize=17, weight='bold', color='#182630')
        labels.append(fig.text(left + .025, .375, '', fontsize=11, color='#435966'))
        hist = fig.add_axes([left + .035, .125, .40, .115])
        series = case['history']
        hist.plot(series['time_ms'], series['reaction_moment_Nmm'] / 1000, color='#246b95', lw=1.7)
        marker, = hist.plot([0], [0], 'o', color='#bf642d', ms=6)
        markers.append(marker)
        hist.set(xlim=(0, series['time_ms'][-1]), xlabel='Simulation time (ms)', ylabel='End moment (N·m)')
        hist.set_title('Computed end reaction', loc='left', fontsize=11)
        hist.grid(axis='y', color='#e3e8eb', lw=.6)
    colorbar = fig.colorbar(dots[0], cax=fig.add_axes([.23, .315, .54, .012]), orientation='horizontal')
    colorbar.ax.xaxis.set_label_position('top')
    colorbar.ax.tick_params(labelsize=9)
    colorbar.set_label('von Mises stress (MPa) | shared fixed full-range scale', fontsize=10)
    fig.text(.04, .045, '80 × 8 × 8 mm · h = 2 mm · horizon = 6 mm · Δt = 0.0005 ms · dynamic relaxation = 8/ms', fontsize=11, color='#435966')
    fig.text(.04, .019, 'Actual saved PD fields · no damage/deletion · no interpolated states · demonstration without an independent spatial reference', fontsize=10, color='#435966')
    width, height = fig.canvas.get_width_height()
    fps = 20
    command = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24',
               '-s', f'{width}x{height}', '-r', str(fps), '-i', '-', '-an', '-c:v', 'libx264', '-preset', 'medium',
               '-crf', '20', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(args.output)]
    logpath = args.output.with_suffix('.ffmpeg.log')
    with logpath.open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stderr=log)
        try:
            for i in range(len(cases[0]['steps'])):
                for col, case in enumerate(cases):
                    frame, row = case['fields'][i], case['rows'][i]
                    dots[col]._offsets3d = (frame['x_mm'], frame['y_mm'], frame['z_mm'])
                    dots[col].set_array(frame['von_mises_MPa'])
                    for line, ids in outlines[col]:
                        line.set_data_3d(frame['x_mm'][ids], frame['y_mm'][ids], frame['z_mm'][ids])
                    labels[col].set_text(f'Angle {row["angle_deg"]:.1f}°   |   min J {row["min_J"]:.3f}   |   max stress {row["max_von_mises_MPa"]:.1f} MPa')
                    markers[col].set_data([row['time_ms']], [row['reaction_moment_Nmm'] / 1000])
                timer.set_text(f't = {cases[0]["rows"][i]["time_ms"]:.3f} ms')
                fig.canvas.draw()
                pixels = np.asarray(fig.canvas.buffer_rgba())[:, :, :3].copy().tobytes()
                repeats = fps if i == 0 else 2 * fps if i == len(cases[0]['steps']) - 1 else 1
                for _ in range(repeats):
                    process.stdin.write(pixels)
                if i in [len(cases[0]['steps']) // 2, len(cases[0]['steps']) - 1]:
                    fig.savefig(args.output.with_name(args.output.stem + f'-frame-{i:03d}.png'), dpi=120)
                if i % 25 == 0:
                    print(f'Rendered {i + 1}/{len(cases[0]["steps"])} actual snapshot states', flush=True)
        finally:
            process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError(logpath.read_text(encoding='utf-8'))
    subprocess.run([ffmpeg, '-v', 'error', '-i', str(args.output), '-f', 'null', '-'], check=True)
    probe = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(args.output)], encoding='utf-8'))
    stream = probe['streams'][0]
    assert stream['codec_name'] == 'h264' and stream['pix_fmt'] == 'yuv420p'
    palette = 'fps=10,scale=960:-1:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff:max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3'
    subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(args.output), '-filter_complex', palette, '-loop', '0', str(args.output.with_suffix('.gif'))], check=True)
    provenance = {'material': 'compressible Neo-Hookean', 'viscosity': 'disabled', 'actual_states_per_case': len(cases[0]['steps']),
                  'simulation_time_ms': [0, float(cases[0]['rows'][-1]['time_ms'])], 'dimensions': [width, height],
                  'playback_seconds': float(probe['format']['duration']), 'stress_scale_MPa': [0, vmax],
                  'time_interpolation': False, 'independent_spatial_validation': False, 'full_decode': 'pass', 'cases': {}}
    for case in cases:
        provenance['cases'][case['meta']['scenario']] = {
            'angle_deg': case['meta']['maximumAngle_deg'],
            'minimum_J_over_history': float(case['history']['min_J'].min()),
            'snapshot_sha256': hashlib.sha256((case['root'] / 'field_snapshots.csv').read_bytes()).hexdigest(),
            'history_sha256': hashlib.sha256((case['root'] / 'timeseries.csv').read_bytes()).hexdigest()}
    provenance['movie_sha256'] = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix('.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
    print('Verified movie:', args.output)

if __name__ == '__main__':
    main()
