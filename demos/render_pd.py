"""Render actual Elastic or Johnson-Cook PD field evolution with plugin provenance."""
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
from pd_results import load_case

REPO = Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--case',choices=['elastic','jc'],default='jc')
    p.add_argument('--output',type=Path)
    a=p.parse_args()
    ffmpeg=shutil.which('ffmpeg');ffprobe=shutil.which('ffprobe')
    if not ffmpeg or not ffprobe: p.error('Install FFmpeg and put ffmpeg and ffprobe on PATH.')
    data=load_case(REPO/'demos/output'/f'{a.case}_pd')
    times,x,dt=data['times'],data['x'],data['meta']['dt_ms']
    secondary='ux' if a.case=='elastic' else 'eqps'
    fields=np.stack([data['stress'],data[secondary]],axis=-1)
    material='Elastic' if a.case=='elastic' else 'Johnson–Cook'
    umax=float(data['history']['load_displacement_mm'][-1])
    if a.output is None: a.output=REPO/'demos/output'/('elastic.mp4' if a.case=='elastic' else 'transient.mp4')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    plt.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Arial','DejaVu Sans'],'font.size':14,
                         'axes.spines.top':False,'axes.spines.right':False,'axes.linewidth':.8})
    fig=plt.figure(figsize=(16,9),dpi=120,facecolor='white')
    fig.text(.065,.956,f'Correspondence PD + {material}',fontsize=22,weight='bold',color='#182630')
    fig.text(.065,.917,'Actual PD solve · material plugin updates · free interior particles',fontsize=17,color='#3b4b55')
    timer=fig.text(.785,.952,'',fontsize=19,weight='bold',color='#182630')
    axes=[fig.add_axes([.065,.661,.70,.183]),fig.add_axes([.065,.403,.70,.183])]
    caxes=[fig.add_axes([.782,.661,.014,.183]),fig.add_axes([.782,.403,.014,.183])]
    limits=[(float(fields[:,:,:,0].min()),float(fields[:,:,:,0].max())),(0,float(fields[:,:,:,1].max()))]
    images=[]
    for k,(ax,cax,title,cmap) in enumerate(zip(axes,caxes,['Axial stress','Axial displacement' if secondary=='ux' else 'Equivalent plastic strain'],['viridis','cividis' if secondary=='ux' else 'magma'])):
        im=ax.pcolormesh(data['x_edges'],data['y_edges'],fields[0,:,:,k],shading='flat',cmap=cmap,norm=Normalize(*limits[k]))
        ax.set_title(title,loc='left',pad=29,weight='bold',fontsize=17)
        ax.set(xlim=(0,100),ylim=(-10,10),xlabel='Reference x (mm)',ylabel='y (mm)',xticks=np.arange(0,101,20),yticks=[-10,0,10])
        ax.text(0,1.05,'Fixed axial end band',transform=ax.transAxes,fontsize=12)
        ax.text(1,1.05,'Prescribed displacement →',transform=ax.transAxes,fontsize=12,ha='right')
        ax.axvspan(0,2,color='white',alpha=.15);ax.axvspan(98,100,color='white',alpha=.15)
        cb=fig.colorbar(im,cax=cax);cb.set_label('MPa' if k==0 else 'mm' if secondary=='ux' else 'strain',fontsize=12);cb.ax.tick_params(labelsize=11)
        images.append(im)
    axes[0].set_xlabel('');axes[0].tick_params(axis='x',labelbottom=False)
    profile=fig.add_axes([.065,.112,.70,.172])
    profile.set_title('PD midplane stress profile | averaged over displayed particles',loc='left',fontsize=15,weight='bold',pad=10)
    actual,=profile.plot(x,fields[0,:,:,0].mean(axis=0),color='#246b95',lw=2.4,label='Current profile')
    reflected,=profile.plot(x,fields[0,:,:,0].mean(axis=0)[::-1],color='#91979b',lw=1.6,ls='--',label='Reflection about x = 50 mm')
    profile.set(xlim=(0,100),ylim=(min(0,limits[0][0]),limits[0][1]*1.04),xlabel='Reference x (mm)',ylabel='Axial stress (MPa)')
    profile.legend(loc='upper right',fontsize=11,ncol=2,frameon=False);profile.grid(axis='y',color='#e4e8eb',lw=.7)
    load=fig.add_axes([.86,.68,.118,.16]);tplot=np.linspace(0,.12,241)
    load.plot(tplot,.5*umax*(1-np.cos(np.pi*tplot/.12)),color='#405d6b',lw=2)
    marker,=load.plot([0],[0],'o',color='#bd6129',ms=7)
    load.set(title='Right-end u (mm)',xlabel='Time (ms)',xlim=(0,.12),ylim=(0,umax*1.025),xticks=[0,.06,.12],yticks=[0,umax/2,umax])
    load.tick_params(labelsize=10);load.xaxis.label.set_size(11);load.title.set_size(13)
    h=data['meta']['spacing_mm']
    for ypos,line in [(.60,'100 × 20 × 4 mm'),(.565,f'h = {h:g} mm'),(.53,f'Δt = {dt*1e6:g} ns'),
                     (.48,'One-sided loading'),(.445,'Left: uₓ = 0'),(.41,'Right: uₓ = u(t)'),
                     (.355,'No damage or deletion'),(.32,f'{material} VUMAT'),(.265,'Reference coordinates'),(.23,'Fixed colour scales'),(.175,'Playback slowed')]:
        fig.text(.858,ypos,line,fontsize=12,color='#3b4b55')
    fig.text(.065,.035,'Actual PD z = 0 particles · reference coordinates · no smoothing · no interpolated simulation states',fontsize=12,color='#53626c')
    width,height=fig.canvas.get_width_height();fps=20
    cmd=[ffmpeg,'-hide_banner','-loglevel','error','-y','-f','rawvideo','-vcodec','rawvideo','-pix_fmt','rgb24','-s',f'{width}x{height}',
         '-r',str(fps),'-i','-','-an','-c:v','libx264','-preset','medium','-crf','20','-pix_fmt','yuv420p','-movflags','+faststart',str(a.output)]
    logpath=a.output.with_suffix('.ffmpeg.log')
    with logpath.open('w',encoding='utf-8') as log:
        process=subprocess.Popen(cmd,stdin=subprocess.PIPE,stderr=log)
        try:
            for i,time in enumerate(times):
                for k,im in enumerate(images): im.set_array(fields[i,:,:,k].ravel())
                actual.set_ydata(fields[i,:,:,0].mean(axis=0));reflected.set_ydata(fields[i,:,:,0].mean(axis=0)[::-1])
                marker.set_data([time],[.5*umax*(1-np.cos(np.pi*time/.12))]);timer.set_text(f't = {time:.4f} ms')
                fig.canvas.draw();rgb=np.asarray(fig.canvas.buffer_rgba())[:,:,:3].copy().tobytes()
                for _ in range(fps if i==0 else 2*fps if i==len(times)-1 else 1): process.stdin.write(rgb)
                if i in [30,len(times)//2,len(times)-1]: fig.savefig(a.output.with_name(a.output.stem+f'-frame-{i:03d}.png'),dpi=120)
                if i%40==0: print(f'Rendered {i+1}/{len(times)} actual fields',flush=True)
        finally: process.stdin.close()
        if process.wait()!=0: raise RuntimeError(logpath.read_text(encoding='utf-8'))
    subprocess.run([ffmpeg,'-v','error','-i',str(a.output),'-f','null','-'],check=True)
    probe=json.loads(subprocess.check_output([ffprobe,'-v','error','-show_streams','-show_format','-of','json',str(a.output)],encoding='utf-8'))
    video=probe['streams'][0]
    assert video['codec_name']=='h264' and video['pix_fmt']=='yuv420p'
    record=dict(data['proof'],case=a.case,spacing_mm=h,dt_ns=dt*1e6,
                playback_seconds=float(probe['format']['duration']),dimensions=[width,height],
                stress_scale_MPa=list(limits[0]),secondary_field=secondary,secondary_scale=list(limits[1]),
                movie_sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),
                smoothing=False,time_interpolation=False,full_decode='pass')
    a.output.with_suffix('.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    palette='fps=10,scale=960:-1:flags=lanczos,split[s0][s1];[s0]palettegen=stats_mode=diff:max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3'
    subprocess.run([ffmpeg,'-v','error','-y','-i',str(a.output),'-filter_complex',palette,'-loop','0',str(a.output.with_suffix('.gif'))],check=True)
    print('Verified movie:',a.output)

if __name__=='__main__':main()
