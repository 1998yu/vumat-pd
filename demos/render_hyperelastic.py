"""Render bending and torsion as separate videos from actual Neo-Hookean snapshots."""
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
from matplotlib.cm import ScalarMappable
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

REPO = Path(__file__).resolve().parents[1]

def boundary_faces(frame):
    """Connect the rectangular reference grid's actual boundary particles."""
    coords = np.column_stack([frame[k] for k in ['x0_mm', 'y0_mm', 'z0_mm']])
    axes = [np.unique(coords[:, d]) for d in range(3)]
    lookup = {tuple(point): i for i, point in enumerate(coords)}
    faces = []
    for fixed in range(3):
        u, v = [d for d in range(3) if d != fixed]
        for edge in [axes[fixed][0], axes[fixed][-1]]:
            for i in range(len(axes[u]) - 1):
                for j in range(len(axes[v]) - 1):
                    corners = []
                    for di, dj in [(0, 0), (1, 0), (1, 1), (0, 1)]:
                        point = np.zeros(3)
                        point[fixed], point[u], point[v] = edge, axes[u][i + di], axes[v][j + dj]
                        corners.append(lookup[tuple(point)])
                    faces.append(corners)
    return np.asarray(faces, dtype=int)

def read_case(root):
    meta = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
    assert meta['solver'] == 'correspondence peridynamics'
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

def render(case, output, norm, ffmpeg, ffprobe, preview_only):
    name = case['meta']['scenario']
    fig = plt.figure(figsize=(12, 8), dpi=120, facecolor='white')
    plt.rcParams.update({'font.family': 'sans-serif', 'font.sans-serif': ['Arial', 'DejaVu Sans'], 'font.size': 12})
    fig.text(.045, .948, f'PD + Neo-Hookean | {name.capitalize()}', fontsize=23, weight='bold', color='#182630')
    fig.text(.045, .906, f'{case["meta"]["maximumAngle_deg"]}° end rotation · fixed base · free interior particles', fontsize=14, color='#435966')
    timer = fig.text(.79, .948, '', fontsize=18, weight='bold', color='#182630')
    status = fig.text(.045, .865, '', fontsize=13, color='#435966')
    ax = fig.add_axes([.02, .225, .96, .605], projection='3d')
    positions = np.concatenate([np.column_stack([frame[k] for k in ['x_mm', 'y_mm', 'z_mm']]) for frame in case['fields']])
    lo, hi = positions.min(axis=0), positions.max(axis=0)
    pad = np.maximum((hi - lo) * .07, 1)
    lo, hi = lo - pad, hi + pad
    for setter, lower, higher in zip([ax.set_xlim, ax.set_ylim, ax.set_zlim], lo, hi):
        setter(lower, higher)
    ax.set_box_aspect(hi - lo, zoom=1.65 if name == 'bending' else 1.8)
    ax.set_proj_type('ortho')
    ax.view_init(elev=28 if name == 'bending' else 22, azim=-64)
    ax.set_axis_off()
    faces = boundary_faces(case['fields'][0])
    cmap = plt.get_cmap('viridis')
    frame = case['fields'][0]
    coords = np.column_stack([frame[k] for k in ['x_mm', 'y_mm', 'z_mm']])
    surface = Poly3DCollection(coords[faces], facecolors=cmap(norm(frame['von_mises_MPa'][faces].mean(axis=1))),
                               edgecolors=(.13, .21, .26, .65), linewidths=.4, zsort='average')
    surface.set_clip_on(False)
    ax.add_collection3d(surface)
    colorbar = fig.colorbar(ScalarMappable(norm=norm, cmap=cmap), cax=fig.add_axes([.22, .17, .56, .014]), orientation='horizontal')
    colorbar.ax.xaxis.set_label_position('top')
    colorbar.set_label('von Mises stress (MPa) | fixed full-range scale', fontsize=12, labelpad=9)
    fig.text(.045, .078, '80 × 8 × 8 mm · h = 2 mm · horizon = 6 mm · dynamic relaxation = 8/ms', fontsize=12, color='#435966')
    fig.text(.045, .048, 'Actual solver fields · true deformation scale · surface facets connect boundary particles', fontsize=11, color='#435966')
    fig.text(.045, .020, 'Face colors average corner stresses · no interpolated time states · no damage/deletion', fontsize=10, color='#435966')

    def update(i):
        frame, state = case['fields'][i], case['rows'][i]
        coords = np.column_stack([frame[k] for k in ['x_mm', 'y_mm', 'z_mm']])
        surface.set_verts(coords[faces])
        surface.set_facecolor(cmap(norm(frame['von_mises_MPa'][faces].mean(axis=1))))
        status.set_text(f'Angle {state["angle_deg"]:.1f}°   ·   min J {state["min_J"]:.3f}   ·   max stress {state["max_von_mises_MPa"]:.1f} MPa'
                        + f'   ·   end moment {state["reaction_moment_Nmm"] / 1000:.2f} N·m')
        timer.set_text(f't = {state["time_ms"]:.3f} ms')
        fig.canvas.draw()

    if preview_only:
        for i in [0, len(case['steps']) // 2, len(case['steps']) - 1]:
            update(i)
            fig.savefig(output.with_name(output.stem + f'-frame-{i:03d}.png'), dpi=120)
        plt.close(fig)
        return
    width, height = fig.canvas.get_width_height()
    fps = 20
    command = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24',
               '-s', f'{width}x{height}', '-r', str(fps), '-i', '-', '-an', '-c:v', 'libx264', '-preset', 'medium',
               '-crf', '20', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(output)]
    logpath = output.with_suffix('.ffmpeg.log')
    with logpath.open('w', encoding='utf-8') as log:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stderr=log)
        try:
            for i in range(len(case['steps'])):
                update(i)
                pixels = np.asarray(fig.canvas.buffer_rgba())[:, :, :3].copy().tobytes()
                repeats = fps if i == 0 else 2 * fps if i == len(case['steps']) - 1 else 1
                for _ in range(repeats):
                    process.stdin.write(pixels)
                if i in [0, len(case['steps']) // 2, len(case['steps']) - 1]:
                    fig.savefig(output.with_name(output.stem + f'-frame-{i:03d}.png'), dpi=120)
                if i % 25 == 0:
                    print(f'{name}: rendered {i + 1}/{len(case["steps"])} actual states', flush=True)
        finally:
            process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError(logpath.read_text(encoding='utf-8'))
    subprocess.run([ffmpeg, '-v', 'error', '-i', str(output), '-f', 'null', '-'], check=True)
    probe = json.loads(subprocess.check_output([ffprobe, '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(output)], encoding='utf-8'))
    assert probe['streams'][0]['codec_name'] == 'h264' and probe['streams'][0]['pix_fmt'] == 'yuv420p'
    palette = 'fps=10,scale=960:-1:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff:max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3'
    subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(output), '-filter_complex', palette, '-loop', '0', str(output.with_suffix('.gif'))], check=True)
    provenance = {'solver': case['meta']['solver'], 'plugin_file': case['meta']['plugin_file'],
                  'material': 'compressible Neo-Hookean', 'scenario': name, 'angle_deg': case['meta']['maximumAngle_deg'],
                  'layout': 'single demo', 'actual_states': len(case['steps']), 'simulation_time_ms': [0, float(case['rows'][-1]['time_ms'])],
                  'dimensions': [width, height], 'playback_seconds': float(probe['format']['duration']),
                  'stress_scale_MPa': [float(norm.vmin), float(norm.vmax)], 'geometry_exaggeration': False,
                  'surface_visualization': 'Facets connect actual boundary particles; face stress is the mean of their corner values.',
                  'time_interpolation': False, 'independent_spatial_validation': False, 'full_decode': 'pass',
                  'minimum_J_over_history': float(case['history']['min_J'].min()),
                  'snapshot_sha256': hashlib.sha256((case['root'] / 'field_snapshots.csv').read_bytes()).hexdigest(),
                  'history_sha256': hashlib.sha256((case['root'] / 'timeseries.csv').read_bytes()).hexdigest(),
                  'movie_sha256': hashlib.sha256(output.read_bytes()).hexdigest()}
    output.with_suffix('.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
    plt.close(fig)
    print('Verified movie:', output)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, default=REPO / 'demos/output/hyperelastic')
    parser.add_argument('--output-dir', type=Path, default=REPO / 'demos/output')
    parser.add_argument('--scenario', choices=['bending', 'torsion', 'both'], default='both')
    parser.add_argument('--preview-only', action='store_true', help='Save initial, middle and final PNGs without encoding movies.')
    args = parser.parse_args()
    ffmpeg, ffprobe = shutil.which('ffmpeg'), shutil.which('ffprobe')
    if not args.preview_only and (not ffmpeg or not ffprobe):
        parser.error('Install FFmpeg and put ffmpeg and ffprobe on PATH.')
    names = ['bending', 'torsion'] if args.scenario == 'both' else [args.scenario]
    cases = [read_case(args.results / name) for name in names]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    vmax = max(float(frame['von_mises_MPa'].max()) for case in cases for frame in case['fields'])
    for case in cases:
        assert np.isclose(case['rows'][-1]['angle_deg'], 150 if case['meta']['scenario'] == 'bending' else 540)
        render(case, args.output_dir / (case['meta']['scenario'] + '.mp4'), Normalize(0, vmax), ffmpeg, ffprobe, args.preview_only)

if __name__ == '__main__':
    main()
