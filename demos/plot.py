"""Plot actual demo outputs or the verified 3D archive."""
from pathlib import Path
import argparse
import json
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize

REPO=Path(__file__).resolve().parents[1]
plt.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Arial','DejaVu Sans'],
                     'font.size':11,'axes.spines.top':False,'axes.spines.right':False,
                     'svg.fonttype':'none','pdf.fonttype':42,'legend.frameon':False})

def read(p):
    if not p.exists(): raise SystemExit(f'Missing result: {p}. Run the corresponding demo first.')
    d=np.genfromtxt(p,delimiter=',',names=True,dtype=None,encoding='utf-8')
    for key in d.dtype.names:
        if d.dtype[key].kind in 'fi' and not np.isfinite(d[key]).all(): raise ValueError(f'Nonfinite {key}')
    return d

def save(fig,out,name,source):
    out.mkdir(parents=True,exist_ok=True)
    fig.savefig(out/f'{name}.png',dpi=180,facecolor='white')
    fig.savefig(out/f'{name}.svg',facecolor='white')
    (out/f'{name}.json').write_text(json.dumps(source,indent=2)+'\n',encoding='utf-8')
    plt.close(fig)
    print('Wrote',out/f'{name}.png')

def elastic(out):
    fig,axes=plt.subplots(1,2,figsize=(10,3.7),layout='constrained')
    checks={}
    for ax,name,label,component in zip(axes,['affine_bar_elastic','affine_shear_elastic'],
                                     ['Affine extension','Simple shear'],['xx','xy']):
        p=REPO/'examples/output'/name
        meta=json.loads((p/'metadata.json').read_text())
        assert meta['status']=='pass'
        d=read(p/'timeseries.csv')
        ax.plot(d['applied_strain'],d[f'elastic_reference_stress_{component}'],color='#85939d',lw=2.4,label='Analytical reference')
        ax.plot(d['applied_strain'],d[f'avg_raw_stress_{component}'],'o',color='#246b95',ms=4,label='PD + elastic VUMAT')
        ax.set(title=label,xlabel='Applied strain' if component=='xx' else 'Applied shear gamma',ylabel='Axial stress (MPa)' if component=='xx' else 'Shear stress (MPa)')
        ax.grid(axis='y',color='#e3e8eb',lw=.7);ax.legend(fontsize=9)
        checks[name]={'status':meta['status'],'maximum_stress_error_MPa':meta['maxReferenceStressError'],'max_net_force_N':meta['maxNetForceNorm']}
    fig.suptitle('Elastic material plugin | extension and shear checks',fontsize=15,weight='bold')
    save(fig,out,'elastic',checks)

def material_point(out):
    p=REPO/'examples/output/jc_material_point'
    d=read(p/'timeseries.csv');meta=json.loads((p/'metadata.json').read_text())
    assert meta['status']=='pass' and d['state_indicator'][-1]>0
    fig,axes=plt.subplots(1,2,figsize=(10,3.7),layout='constrained')
    axes[0].plot(d['applied_strain'],d['stress'],color='#246b95',lw=2.3)
    axes[0].set(xlabel='Prescribed axial strain',ylabel='Axial stress (MPa)',title='Constitutive response')
    axes[1].plot(d['applied_strain'],d['state_indicator'],color='#b66834',lw=2.3)
    axes[1].set(xlabel='Prescribed axial strain',ylabel='Equivalent plastic strain',title='Persistent material state')
    for ax in axes:
        ax.grid(axis='y',color='#e3e8eb',lw=.7)
        ax.set_xticks(np.linspace(0,.02,6))
    fig.suptitle('Johnson–Cook plugin | material-point update',fontsize=15,weight='bold')
    save(fig,out,'material-point',{'case':'jc_material_point','condition':'zero lateral y stress; constrained z strain',
                                  'eqps_state_index_zero_based':1,'final_stress_MPa':float(d['stress'][-1]),'final_eqps':float(d['state_indicator'][-1])})

def pd_grid(d,key):
    d=d[np.isclose(d['z0_mm'],0,atol=1e-8)]
    x=np.unique(d['x0_mm']);y=np.unique(d['y0_mm'])
    order=np.lexsort((d['x0_mm'],d['y0_mm']))
    def edges(a): return np.r_[a[0],.5*(a[:-1]+a[1:]),a[-1]]
    return edges(x),edges(y),d[key][order].reshape(len(y),len(x))

def fem_grid(d,key):
    zmin=np.abs(d['z0_mm']).min();d=d[np.isclose(np.abs(d['z0_mm']),zmin,atol=1e-8)]
    xc=np.round(d['x0_mm'],10);yc=np.round(d['y0_mm'],10)
    x=np.unique(xc);y=np.unique(yc)
    sums=np.zeros((len(y),len(x)));counts=sums.copy()
    ix=np.searchsorted(x,xc);iy=np.searchsorted(y,yc)
    np.add.at(sums,(iy,ix),d[key]);np.add.at(counts,(iy,ix),1)
    assert np.all(counts==2)
    return np.linspace(0,100,len(x)+1),np.linspace(-10,10,len(y)+1),sums/counts

def tensile(out,source,resolution):
    if source=='archive':
        sys.path.insert(0,str(REPO/'benchmarks/tensile3d'))
        from unpack_archive import unpack
        results=unpack();resolution=resolution or 'fine'
    else: results=REPO/'benchmarks/tensile3d/generated/results';resolution=resolution or 'coarse'
    names=['pd_shared','fem_fine'] if resolution=='fine' else ['pd_coarse','fem_coarse']
    fields=[read(results/n/('field_snapshots.csv' if n.startswith('pd') else 'final_gauss_points.csv')) for n in names]
    series=[read(results/n/'timeseries.csv') for n in names]
    fig=plt.figure(figsize=(11,7.8),layout='constrained')
    gs=fig.add_gridspec(3,2,height_ratios=[1,1,1.4],hspace=.12)
    all_s=np.concatenate([d['axial_stress_MPa' if n.startswith('pd') else 's11_MPa'] for d,n in zip(fields,names)])
    all_e=np.concatenate([d['eqps'] for d in fields])
    for row,variable in enumerate(['stress','eqps']):
        axes=[];norm=Normalize(float(all_s.min()),float(all_s.max())) if row==0 else Normalize(0,float(all_e.max()))
        for col,(d,n,method) in enumerate(zip(fields,names,['PD','FEM'])):
            key=('axial_stress_MPa' if n.startswith('pd') else 's11_MPa') if row==0 else 'eqps'
            x,y,grid=pd_grid(d,key) if method=='PD' else fem_grid(d,key)
            ax=fig.add_subplot(gs[row,col]);mesh=ax.pcolormesh(x,y,grid,norm=norm,cmap='viridis' if row==0 else 'magma',shading='flat',rasterized=True)
            ax.set(xlim=(0,100),ylim=(-10,10),xlabel='Reference x (mm)',ylabel='y (mm)',title=f'{method} | '+('axial stress' if row==0 else 'equivalent plastic strain'))
            ax.set_aspect('equal');axes.append(ax)
        fig.colorbar(mesh,ax=axes,shrink=.85,label='MPa' if row==0 else 'strain',pad=.02)
    ax=fig.add_subplot(gs[2,:])
    for s,label,color in zip(series,['PD','FEM'],['#246b95','#b66834']):
        ax.plot(s['time_ms'],s['average_axial_stress_MPa'],color=color,lw=2,label=label)
    ax.set(xlim=(0,.12),xlabel='Simulation time (ms)',ylabel='Mean axial stress (MPa)',title='Shared material routine; independent spatial solvers')
    ax.legend(loc='lower right');ax.grid(axis='y',color='#e3e8eb',lw=.7)
    fig.suptitle(f'3D tensile comparison | {resolution} '+('verified archive' if source=='archive' else 'new solver outputs'),fontsize=16,weight='bold')
    save(fig,out,'tensile',{'data_source':source,'resolution':resolution,'cases':names,'coordinates':'reference',
                          'FEM_sampling':'mean of nearest two Gauss planes; quadrature subcells','PD_sampling':'z=0 nodal layer',
                          'smoothing':False,'shared_full_range_scales':True})

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--demo',choices=['elastic','material-point','tensile'],required=True)
    p.add_argument('--results',choices=['archive','generated'],default='archive',help='Tensile data source.')
    p.add_argument('--resolution',choices=['coarse','fine'])
    p.add_argument('--output-dir',type=Path,default=REPO/'demos/output')
    a=p.parse_args()
    if a.demo=='elastic':elastic(a.output_dir)
    elif a.demo=='material-point':material_point(a.output_dir)
    else:tensile(a.output_dir,a.results,a.resolution)

if __name__=='__main__':main()
