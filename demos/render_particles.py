"""Render all four PD demos as discrete particles at actual deformed positions."""
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
from pd_results import load_case

REPO = Path(__file__).resolve().parents[1]
CASES = ['elastic', 'jc', 'bending', 'torsion']
FOLDERS = dict(elastic='elastic-tension', jc='johnson-cook-tension',
               bending='neohookean-bending', torsion='neohookean-torsion')


def read_case(name, results):
    if name in ['elastic', 'jc']:
        d = load_case(results / f'{name}_pd')
        history = d['history']
        return dict(name=name, title='Elastic tension' if name == 'elastic' else 'Johnson–Cook plasticity',
                    material='Elastic' if name == 'elastic' else 'Johnson–Cook',
                    times=d['times'], positions=d['particle_positions'], reference=d['reference_particles'],
                    values=d['particle_stress'], quantity='Axial stress (MPa)',
                    second=d['particle_eqps'].max(axis=1),
                    history=history, curve_x=history['time_ms'],
                    curve_y=history['average_axial_stress_MPa'], curve_label='Mean axial stress (MPa)',
                    second_x=history['time_ms'],
                    second_y=history['load_displacement_mm'] if name == 'elastic' else history['max_eqps'],
                    second_label='End displacement (mm)' if name == 'elastic' else 'Maximum plastic strain',
                    subtitle='Small-strain material reuse' if name == 'elastic' else 'Stress and persistent plastic history',
                    parameters='100 × 20 × 4 mm  ·  h = 2 mm  ·  horizon = 6 mm',
                    end_value=history['load_displacement_mm'][-1], proof=d['proof'])
    root = results / 'hyperelastic' / name
    meta = json.loads((root / 'metadata.json').read_text(encoding='utf-8'))
    if meta.get('solver') != 'correspondence peridynamics':
        raise ValueError('Only actual PD outputs can be rendered.')
    raw = np.genfromtxt(root / 'field_snapshots.csv', delimiter=',', names=True)
    history = np.genfromtxt(root / 'timeseries.csv', delimiter=',', names=True)
    assert meta['status'] == 'completed' and np.all(history['finite'] == 1)
    assert np.all(history['min_J'] > 0)
    for table in [raw, history]:
        assert all(np.isfinite(table[key]).all() for key in table.dtype.names)
    frames = raw.reshape(-1, meta['particles'])
    assert np.all(frames['step'] == frames['step'][:, :1])
    assert np.all(frames['time_ms'] == frames['time_ms'][:, :1])
    assert np.array_equal(frames['particle'], np.broadcast_to(frames['particle'][0], frames['particle'].shape))
    positions = np.stack([frames[key] for key in ['x_mm', 'y_mm', 'z_mm']], axis=-1)
    reference = np.stack([frames[key][0] for key in ['x0_mm', 'y0_mm', 'z0_mm']], axis=-1)
    assert all(np.all(frames[key] == frames[key][0]) for key in ['x0_mm', 'y0_mm', 'z0_mm'])
    proof = dict(solver=meta['solver'], plugin_file=meta['plugin_file'], particles=meta['particles'],
                 actual_states=len(frames), simulation_time_ms=[float(frames['time_ms'][0, 0]), float(frames['time_ms'][-1, 0])],
                 snapshots_sha256=hashlib.sha256((root / 'field_snapshots.csv').read_bytes()).hexdigest(),
                 history_sha256=hashlib.sha256((root / 'timeseries.csv').read_bytes()).hexdigest())
    return dict(name=name, title='Neo-Hookean bending' if name == 'bending' else 'Neo-Hookean torsion',
                material='Neo-Hookean', times=frames['time_ms'][:, 0], positions=positions, reference=reference,
                values=frames['von_mises_MPa'], quantity='von Mises stress (MPa)',
                history=history, curve_x=history['time_ms'], curve_y=history['angle_deg'],
                curve_label='End rotation (degrees)', second_x=history['time_ms'],
                second_y=history['reaction_moment_Nmm'] / 1000, second_label='End moment (N·m)',
                subtitle='150° bending · finite-deformation demo' if name == 'bending' else '540° torsion · finite-deformation demo',
                parameters='80 × 8 × 8 mm  ·  h = 2 mm  ·  horizon = 6 mm',
                end_value=meta['maximumAngle_deg'], proof=proof)


def projection():
    """Fixed orthographic camera; rotation only, with equal physical scale."""
    elevation, azimuth = np.deg2rad([24, -68])
    view = np.array([np.cos(elevation)*np.cos(azimuth), np.cos(elevation)*np.sin(azimuth), np.sin(elevation)])
    right = np.array([-np.sin(azimuth), np.cos(azimuth), 0])
    up = np.cross(view, right)
    return np.stack([right, up, view], axis=1)


def scene(case):
    plt.rcParams.update({'font.family':'sans-serif', 'font.sans-serif':['Arial','DejaVu Sans'],
                         'font.size':12, 'svg.fonttype':'none', 'axes.spines.top':False,
                         'axes.spines.right':False, 'axes.linewidth':.7})
    ink, muted, blue = '#172b3a', '#526677', '#237798'
    fig = plt.figure(figsize=(14, 8.75), dpi=120, facecolor='white')
    fig.text(.045,.951,'VUMAT–PD / PARTICLE DEMOS',fontsize=11,weight='bold',color=blue)
    fig.text(.045,.905,case['title'],fontsize=28,weight='bold',color=ink)
    fig.text(.045,.867,case['subtitle'],fontsize=15,color=muted)
    timer=fig.text(.78,.916,'',fontsize=18,weight='bold',color=ink)
    fig.text(.78,.877,'Correspondence PD',fontsize=12,color=muted)
    ax=fig.add_axes([.025,.32,.77,.48])
    camera=projection()
    projected=case['positions'] @ camera
    reference=case['reference'] @ camera
    lo=projected[:,:,:2].min(axis=(0,1)); hi=projected[:,:,:2].max(axis=(0,1))
    if case['name'] in ['bending','torsion']:
        lo=np.minimum(lo,reference[:,:2].min(axis=0));hi=np.maximum(hi,reference[:,:2].max(axis=0))
        ax.scatter(reference[:,0],reference[:,1],s=15,c='#dce4eb',alpha=.35,linewidths=0,zorder=0)
    pad=np.maximum((hi-lo)*.065,1.2)
    ax.set(xlim=(lo[0]-pad[0],hi[0]+pad[0]),ylim=(lo[1]-pad[1],hi[1]+pad[1]))
    ax.set_aspect('equal',adjustable='box');ax.axis('off')
    low=float(case['values'].min()); high=float(case['values'].max())
    norm=Normalize(min(0,low),max(high,1e-12)); cmap=plt.get_cmap('viridis')
    size=58 if case['name'] in ['elastic','jc'] else 66
    particles=ax.scatter([],[],s=size,linewidths=.32,edgecolors='#f5f8fc',zorder=2)
    colorbar=fig.colorbar(ScalarMappable(norm=norm,cmap=cmap),cax=fig.add_axes([.822,.387,.017,.365]))
    colorbar.set_label(case['quantity'],fontsize=12,labelpad=10)
    colorbar.ax.tick_params(labelsize=10)
    fig.text(.045,.31,case['parameters'],fontsize=12,color=muted)
    fig.text(.045,.279,f'{case["proof"]["particles"]:,} PD particles · deformed positions · true deformation scale',fontsize=12,color=muted)
    state=fig.text(.81,.315,'',fontsize=12,weight='bold',color=ink)
    fig.text(.81,.284,'Fixed full-range color scale',fontsize=10,color=muted)
    curves=[]
    for rect,x,y,label in [([.075,.098,.40,.122],case['curve_x'],case['curve_y'],case['curve_label']),
                            ([.57,.098,.36,.122],case['second_x'],case['second_y'],case['second_label'])]:
        plot=fig.add_axes(rect)
        plot.plot(x,y,color='#d9e3ea',lw=2)
        line,=plot.plot([],[],color=blue,lw=2.2)
        point,=plot.plot([],[],'o',color=blue,ms=5)
        lower,upper=float(y.min()),float(y.max());span=max(upper-lower,abs(upper)*.05,1e-9)
        plot.set(xlim=(float(x.min()),float(x.max())),ylim=(lower-.07*span,upper+.1*span),xlabel='Time (ms)')
        plot.set_title(label,loc='left',fontsize=12,weight='bold',color=ink,pad=9)
        plot.tick_params(labelsize=10);plot.grid(axis='y',color='#e7edf1',lw=.6)
        curves.append((line,point,x,y))
    fig.text(.045,.022,'One colored point = one PD particle · no surface mesh · no field smoothing · saved solver states only',fontsize=10,color=muted)
    triad_origin=lo+np.array([.05,.02])*(hi-lo)
    for index,label in enumerate(['x','y','z']):
        direction=camera[index,:2]*max(float(hi[0]-lo[0])*.075,3)
        ax.annotate('',xy=triad_origin+direction,xytext=triad_origin,arrowprops=dict(arrowstyle='->',color=muted,lw=.9),zorder=4)
        ax.text(*(triad_origin+direction*1.2),label,fontsize=9,color=muted)

    def update(i):
        xy=projected[i];order=np.argsort(xy[:,2],kind='stable')
        particles.set_offsets(xy[order,:2]);particles.set_facecolors(cmap(norm(case['values'][i,order])))
        time=float(case['times'][i]);timer.set_text(f't = {time:.4f} ms')
        if case['name']=='jc':state.set_text(f'Max plastic strain\n{case["second"][i]:.4f}')
        elif case['name']=='elastic':state.set_text(f'End displacement\n{np.interp(time,case["second_x"],case["second_y"]):.4f} mm')
        else:state.set_text(f'End rotation\n{np.interp(time,case["curve_x"],case["curve_y"]):.1f}°')
        for line,point,x,y in curves:
            cut=x<=time+1e-12;line.set_data(x[cut],y[cut]);point.set_data([time],[np.interp(time,x,y)])
        fig.canvas.draw()
    return fig, update, norm


def render(case, output, preview_only):
    output.mkdir(parents=True,exist_ok=True)
    fig,update,norm=scene(case)
    for index in [0,len(case['times'])//2,len(case['times'])-1]:
        update(index);fig.savefig(output/f'frame-{index:03d}.png',dpi=120)
    fig.savefig(output/'overview.png',dpi=120)
    fig.savefig(output/'overview.svg')
    svg=output/'overview.svg'
    svg.write_text('\n'.join(line.rstrip() for line in svg.read_text(encoding='utf-8').splitlines())+'\n',encoding='utf-8')
    if preview_only:
        plt.close(fig);print('Particle preview:',output,flush=True);return
    ffmpeg,ffprobe=shutil.which('ffmpeg'),shutil.which('ffprobe')
    if not ffmpeg or not ffprobe:raise RuntimeError('Install FFmpeg and put ffmpeg and ffprobe on PATH.')
    movie=output/'particles.mp4';width,height=fig.canvas.get_width_height();fps=20
    command=[ffmpeg,'-v','error','-y','-f','rawvideo','-pix_fmt','rgb24','-s',f'{width}x{height}',
             '-r',str(fps),'-i','-','-an','-c:v','libx264','-preset','medium','-crf','18',
             '-pix_fmt','yuv420p','-movflags','+faststart',str(movie)]
    with (output/'ffmpeg.log').open('w',encoding='utf-8') as log:
        process=subprocess.Popen(command,stdin=subprocess.PIPE,stderr=log)
        try:
            for index in range(len(case['times'])):
                update(index);pixels=np.asarray(fig.canvas.buffer_rgba())[:,:,:3].copy().tobytes()
                repeats=fps if index==0 else 2*fps if index==len(case['times'])-1 else 1
                for _ in range(repeats):process.stdin.write(pixels)
                if index%50==0:print(f'{case["name"]}: {index+1}/{len(case["times"])} particle states',flush=True)
        finally:process.stdin.close()
        if process.wait():raise RuntimeError((output/'ffmpeg.log').read_text(encoding='utf-8'))
    subprocess.run([ffmpeg,'-v','error','-i',str(movie),'-f','null','-'],check=True)
    info=json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(movie)],encoding='utf-8'))
    assert info['streams'][0]['pix_fmt']=='yuv420p'
    palette='fps=10,scale=1120:-1:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff:max_colors=192[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3'
    subprocess.run([ffmpeg,'-v','error','-y','-i',str(movie),'-filter_complex',palette,'-loop','0',str(output/'particles.gif')],check=True)
    proof=dict(case['proof'],case=case['name'],material=case['material'],
               visualization='All actual PD particle centers at deformed 3D coordinates; orthographic depth-sorted point markers.',
               submitted_particle_count=case['proof']['particles'],marker_diameter='Display size only; not physical particle volume.',
               reference_ghost=case['name'] in ['bending','torsion'],surface_mesh=False,field_smoothing=False,
               time_interpolation=False,geometry_exaggeration=False,camera_fixed=True,
               stress_scale_MPa=[float(norm.vmin),float(norm.vmax)],quantity=case['quantity'],
               dimensions=[width,height],playback_seconds=float(info['format']['duration']),
               full_decode='pass',movie_sha256=hashlib.sha256(movie.read_bytes()).hexdigest())
    (output/'particles.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    plt.close(fig);print('Verified particle movie:',movie,flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=CASES+['all'],default='all')
    parser.add_argument('--results',type=Path,default=REPO/'demos/output')
    parser.add_argument('--output-dir',type=Path,default=REPO/'demos/output/particles')
    parser.add_argument('--preview-only',action='store_true')
    args=parser.parse_args()
    for name in CASES if args.case=='all' else [args.case]:
        render(read_case(name,args.results),args.output_dir/FOLDERS[name],args.preview_only)


if __name__=='__main__':main()
