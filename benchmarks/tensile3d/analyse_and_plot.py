from pathlib import Path
import sys, json, hashlib, argparse

R=Path(__file__).resolve().parent
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize

parser=argparse.ArgumentParser(description='Recompute validation metrics and comparison plots.')
parser.add_argument('--results',type=Path,help='Use regenerated solver outputs instead of the archive.')
args=parser.parse_args()
if args.results is None:
    from unpack_archive import unpack
    RESULT=unpack()
else:
    RESULT=args.results.resolve()
ROOT=R
R=ROOT/'generated'
R.mkdir(exist_ok=True)
OUT=R/'figures';OUT.mkdir(exist_ok=True)
names=['fem_coarse','fem_fine','fem_reference','fem_timecheck','pd_coarse','pd_shared','pd_m1p5','pd_delta4','pd_delta2','pd_dt50','pd_dt12p5']
def read(p):return np.genfromtxt(p,delimiter=',',names=True,dtype=None,encoding='utf-8')
data={};hashes={}
for name in names:
    p=RESULT/name
    meta=json.loads((p/'metadata.json').read_text())
    if name.startswith('pd'):assert meta['status']=='pass' and meta['planeStrain'] is False
    else:assert meta['finite'] and meta['planeStrain'] is False and meta['volume_mm3']==8000
    ts=read(p/'timeseries.csv');assert abs(ts['time_ms'][-1]-.12)<1e-10
    assert np.all(ts['finite']==1)
    field=read(p/('field_snapshots.csv' if name.startswith('pd') else 'final_gauss_points.csv'))
    nodes=field if name.startswith('pd') else read(p/'final_nodes.csv')
    for arr in (ts,field,nodes):
        for k in arr.dtype.names:
            if arr.dtype[k].kind in 'fi':assert np.isfinite(arr[k]).all(),(name,k)
    assert not np.isclose(nodes['uz_mm'].min(),0,atol=1e-5),(name,'out-of-plane motion suppressed')
    assert np.isclose(np.ptp(nodes['z0_mm']),4)
    data[name]={'series':ts,'fields':field,'nodes':nodes,'metadata':meta}
    for f in p.glob('*.csv'):hashes[f'{name}/{f.name}']=hashlib.sha256(f.read_bytes()).hexdigest()

common_t=np.arange(1,241)*.0005
coords=np.array([(x,y,z) for x in np.arange(4,98,2) for y in np.arange(-10,12,2) for z in [-2,0,2]],dtype=float)
assert len(coords)==1551
def interpolate_nodes(a,query):
    axes=[np.unique(a[k]) for k in ['x0_mm','y0_mm','z0_mm']]
    order=np.lexsort((a['z0_mm'],a['y0_mm'],a['x0_mm']))
    grid=np.column_stack([a[k][order] for k in ['ux_mm','uy_mm','uz_mm']]).reshape(*(len(t) for t in axes),3)
    idx=[];fraction=[]
    for d,ax in enumerate(axes):
        j=np.clip(np.searchsorted(ax,query[:,d],side='right')-1,0,len(ax)-2)
        t=(query[:,d]-ax[j])/(ax[j+1]-ax[j]);assert t.min()>-1e-8 and t.max()<1+1e-8
        idx.append(j);fraction.append(t)
    out=np.zeros((len(query),3))
    for i in range(2):
        for j in range(2):
            for k in range(2):
                w=np.ones(len(query))
                for d,bit in enumerate([i,j,k]):w*=fraction[d] if bit else 1-fraction[d]
                out+=w[:,None]*grid[idx[0]+i,idx[1]+j,idx[2]+k]
    return out
ref=data['fem_reference'];refs=np.interp(common_t,ref['series']['time_ms'],ref['series']['average_axial_stress_MPa'])
peak=np.max(abs(refs));refeq=ref['series']['max_eqps'][-1]
refU=interpolate_nodes(ref['nodes'],coords)
metrics={}
for name,d in data.items():
    ts=d['series'];stress=np.interp(common_t,ts['time_ms'],ts['average_axial_stress_MPa'])
    u=interpolate_nodes(d['nodes'],coords)
    metrics[name]={'stress_MPa':float(ts['average_axial_stress_MPa'][-1]),'mean_axial_strain':float(ts['average_axial_strain'][-1]),
        'max_eqps':float(ts['max_eqps'][-1]),'stress_history_pct':float(100*np.sqrt(np.mean((stress-refs)**2))/peak),
        'displacement_pct':float(100*np.sqrt(np.mean(np.sum((u-refU)**2,axis=1)))/2),
        'max_eqps_pct':float(100*abs(ts['max_eqps'][-1]-refeq)/refeq),
        'final_stress_pct':float(100*abs(ts['average_axial_stress_MPa'][-1]-metrics.get('fem_reference',{'stress_MPa':ref['series']['average_axial_stress_MPa'][-1]})['stress_MPa'])/abs(ref['series']['average_axial_stress_MPa'][-1])),
        'nodes':len(d['nodes'])}
timecheck={}
for k in ['stress_MPa','max_eqps']:
    timecheck[k+'_difference_pct']=100*abs(metrics['fem_fine'][k]-metrics['fem_timecheck'][k])/abs(metrics['fem_timecheck'][k])
timecheck['history_nrmse_pct']=100*np.sqrt(np.mean((np.interp(common_t,data['fem_fine']['series']['time_ms'],data['fem_fine']['series']['average_axial_stress_MPa'])-np.interp(common_t,data['fem_timecheck']['series']['time_ms'],data['fem_timecheck']['series']['average_axial_stress_MPa']))**2))/np.max(abs(data['fem_timecheck']['series']['average_axial_stress_MPa']))
coarse=data['fem_coarse']['series']
analysis={'reference':'fem_reference','reference_h_mm':2/3,'reference_dt_ns':25,'domain_mm':[100,20,4],
          'interior_mean_region_x_mm':[2,98],'common_output_times':240,'displacement_coordinates':1551,
          'displacement_sampling':'x=4:2:96 mm, y=-10:2:10 mm, z=-2,0,2 mm; trilinear FEM nodal interpolation; all three components',
          'metrics':metrics,'fem_time_step_check':timecheck,'hashes':hashes}
if (RESULT/'fem_symmetry_check/timeseries.csv').exists():
    sym=read(RESULT/'fem_symmetry_check/timeseries.csv')
    sym_nodes=read(RESULT/'fem_symmetry_check/final_nodes.csv')
    def sorted_u(nodes):
        idx=np.lexsort((nodes['z0_mm'],nodes['y0_mm'],nodes['x0_mm']))
        return np.column_stack([nodes[k][idx] for k in ['ux_mm','uy_mm','uz_mm']])
    assert len(sym_nodes)==len(data['fem_coarse']['nodes'])
    symmetry_check={
        'stress_history_nrmse_pct':float(100*np.sqrt(np.mean((sym['average_axial_stress_MPa']-coarse['average_axial_stress_MPa'])**2))/np.max(abs(coarse['average_axial_stress_MPa']))),
        'max_history_stress_difference_pct_of_peak':float(100*np.max(abs(sym['average_axial_stress_MPa']-coarse['average_axial_stress_MPa']))/np.max(abs(coarse['average_axial_stress_MPa']))),
        'max_history_plastic_difference_pct_of_peak':float(100*np.max(abs(sym['max_eqps']-coarse['max_eqps']))/np.max(abs(coarse['max_eqps']))),
        'displacement_rmse_pct':float(100*np.sqrt(np.mean(np.sum((sorted_u(sym_nodes)-sorted_u(data['fem_coarse']['nodes']))**2,axis=1)))/2)}
    assert symmetry_check['max_history_stress_difference_pct_of_peak']<.05
    assert symmetry_check['max_history_plastic_difference_pct_of_peak']<.05
    assert symmetry_check['displacement_rmse_pct']<.001
    analysis['fem_symmetry_check']=symmetry_check
analysis['pd_early_elastic_diagnostic']={name:{
    'measured_early_slope_MPa':data[name]['metadata']['materials'][0]['earlyElasticSlope_MPa'],
    'three_dimensional_uniaxial_reference_MPa':210000,
    'relative_difference_pct':100*abs(data[name]['metadata']['materials'][0]['earlyElasticSlope_MPa']-210000)/210000}
    for name in names if name.startswith('pd')}
analysis['legacy_metadata_note']='Raw PD metadata retains the historical plane-strain elastic tangent diagnostic. It is not used as the analytic reference for the revised free-sided 3D case; the uniaxial E=210000 MPa diagnostic is recomputed here.'
# Compare FEM and PD directly at the same resolution.
direct={}
for label,pdname,femname in [('coarse','pd_coarse','fem_coarse'),('fine','pd_shared','fem_fine')]:
    pdmetrics=metrics[pdname];fm=metrics[femname]
    pair={k:100*abs(pdmetrics[k]-fm[k])/abs(pdmetrics[k]) for k in ['stress_MPa','mean_axial_strain','max_eqps']}
    ps=np.interp(common_t,data[pdname]['series']['time_ms'],data[pdname]['series']['average_axial_stress_MPa'])
    fs=np.interp(common_t,data[femname]['series']['time_ms'],data[femname]['series']['average_axial_stress_MPa'])
    pair['stress_history_pct']=float(100*np.sqrt(np.mean((ps-fs)**2))/np.max(abs(ps)))
    pu=interpolate_nodes(data[pdname]['nodes'],coords);fu=interpolate_nodes(data[femname]['nodes'],coords)
    pair['displacement_pct']=float(100*np.sqrt(np.mean(np.sum((pu-fu)**2,axis=1)))/2)
    direct[label]=pair
(R/'direct_pair_metrics.json').write_text(json.dumps(direct,indent=2))

(R/'analysis.json').write_text(json.dumps(analysis,indent=2))
np.savetxt(R/'displacement_sample_coordinates.csv',coords,delimiter=',',header='x_mm,y_mm,z_mm',comments='')
with (R/'comparison_metrics.csv').open('w') as f:
    fields=list(metrics['fem_reference']);f.write('case,'+','.join(fields)+'\n')
    for name,m in metrics.items():f.write(name+','+','.join(str(m[k]) for k in fields)+'\n')

mpl.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Arial','DejaVu Sans'],'font.size':8,'axes.labelsize':8,
    'xtick.labelsize':7.5,'ytick.labelsize':7.5,'legend.fontsize':7.2,'axes.spines.top':False,'axes.spines.right':False,
    'axes.linewidth':.7,'pdf.fonttype':42,'svg.fonttype':'none'})
def save(fig,name):
    for ext in ['svg','pdf','png','tiff']:fig.savefig(OUT/f'{name}.{ext}',dpi=600,facecolor='white')
    fig.savefig(OUT/f'{name}_preview.png',dpi=160,facecolor='white');plt.close(fig)
def pd_grid(a,field):
    a=a[np.isclose(a['z0_mm'],0,atol=1e-8)]
    xs=np.unique(a['x0_mm']);ys=np.unique(a['y0_mm']);order=np.lexsort((a['x0_mm'],a['y0_mm']))
    val=a[field][order].reshape(len(ys),len(xs))
    def edge(ax):return np.r_[ax[0],.5*(ax[:-1]+ax[1:]),ax[-1]]
    return edge(xs),edge(ys),val
def fem_grid(a,field,h):
    zmin=np.min(abs(a['z0_mm']));sel=a[np.isclose(abs(a['z0_mm']),zmin,atol=1e-8)]
    xc=np.round(sel['x0_mm'],10);yc=np.round(sel['y0_mm'],10)
    xs=np.unique(xc);ys=np.unique(yc)
    values=np.zeros((len(ys),len(xs)));counts=values.copy()
    ix=np.searchsorted(xs,xc);iy=np.searchsorted(ys,yc)
    np.add.at(values,(iy,ix),sel[field]);np.add.at(counts,(iy,ix),1)
    assert np.all(counts==2)
    return np.linspace(0,100,len(xs)+1),np.linspace(-10,10,len(ys)+1),values/counts
contour_names=['pd_coarse','pd_shared','fem_coarse','fem_fine']
stress_vals=np.concatenate([data[n]['fields']['axial_stress_MPa' if n.startswith('pd') else 's11_MPa'] for n in contour_names])
eq_vals=np.concatenate([data[n]['fields']['eqps'] for n in contour_names])
limits=[(float(stress_vals.min()),float(stress_vals.max())),(float(eq_vals.min()),float(eq_vals.max()))]
for num,h,pdname,femname in [(4,2,'pd_coarse','fem_coarse'),(5,1,'pd_shared','fem_fine')]:
    fig=plt.figure(figsize=(183/25.4,103/25.4))
    axes=np.array([[fig.add_axes([x,y,.405,.16]) for x in [.075,.58]] for y in [.72,.27]])
    for row,var in enumerate(['stress','eqps']):
        norm=Normalize(*limits[row]);mesh=None
        for col,name in enumerate([pdname,femname]):
            a=data[name]['fields'];isPD=name.startswith('pd');field=('axial_stress_MPa' if isPD else 's11_MPa') if row==0 else 'eqps'
            xs,ys,grid=pd_grid(a,field) if isPD else fem_grid(a,field,h)
            ax=axes[row,col];mesh=ax.pcolormesh(xs,ys,grid,cmap='viridis' if row==0 else 'magma',norm=norm,rasterized=True,shading='flat')
            ax.set_aspect('equal');ax.set(xlim=(0,100),ylim=(-10,10),xlabel='x (mm)',ylabel='y (mm)')
            letter='abcd'[row*2+col];method='3D PD' if isPD else '3D FEM'
            value=metrics[name]['stress_MPa' if row==0 else 'max_eqps']
            stat=f'mean = {value:.2f} MPa' if row==0 else f'global max = {value:.5f}'
            ax.set_title(f'({letter}) {method}   {stat}',loc='left',fontweight='bold',pad=7,fontsize=8)
        cax=fig.add_axes([.26,.575 if row==0 else .115,.49,.018]);cb=fig.colorbar(mesh,cax=cax,orientation='horizontal')
        cb.set_label('Axial Cauchy stress (MPa)' if row==0 else 'Equivalent plastic strain',fontsize=8,labelpad=2)
    fig.suptitle(f'Fixed thickness 4 mm; h = {h:g} mm; t = 0.12 ms',fontsize=9,y=.975)
    save(fig,'coarse_comparison' if h==2 else 'fine_comparison')

fig,axes=plt.subplots(2,3,figsize=(183/25.4,146/25.4));fig.subplots_adjust(left=.08,right=.965,bottom=.22,top=.88,wspace=.44,hspace=.78)
series=[(['pd_m1p5','pd_shared'],['h = 2','h = 1'],'Grid refinement','delta = 3 mm; dt = 25 ns'),
        (['pd_delta2','pd_shared','pd_delta4'],['delta = 2','delta = 3','delta = 4'],'Horizon variation','h = 1 mm; dt = 25 ns'),
        (['pd_dt50','pd_shared','pd_dt12p5'],['dt = 50','dt = 25','dt = 12.5'],'Time-step variation','h = 1 mm; delta = 3 mm')]
linecols=['#286391','#C77E3D','#6C6C6C'];metriccols=['#286391','#C77E3D','#6C6C6C']
source=[]
for col,(cases,labels,title,condition) in enumerate(series):
    ax=axes[0,col];low=axes[1,col]
    ax.plot(common_t,refs,color='black',lw=1.7,label='FEM reference')
    for i,name in enumerate(cases):
        ts=data[name]['series'];ax.plot(ts['time_ms'],ts['average_axial_stress_MPa'],color=linecols[i],ls=['-','--',':'][i],lw=1.1,label=labels[i])
    ax.set_title(f'({"abc"[col]}) {title}',loc='left',fontweight='bold',pad=23,fontsize=8.5)
    ax.text(0,1.065,condition,transform=ax.transAxes,fontsize=7.3)
    ax.set(xlim=(0,.12),xlabel='Time (ms)',ylabel='Mean axial stress (MPa)');ax.set_xticks([0,.04,.08,.12]);ax.set_ylim(bottom=0)
    ax.legend(frameon=False,loc='lower right',fontsize=6.8)
    compared=['fem_coarse','fem_fine']+cases;x=np.arange(len(compared))
    for k,(field,legend) in enumerate([('stress_history_pct','Stress history'),('displacement_pct','Displacement field'),('max_eqps_pct','Maximum plastic strain')]):
        vals=[metrics[n][field] for n in compared]
        low.bar(x+(k-1)*.24,vals,width=.23,color=metriccols[k],label=legend)
        for n,v in zip(compared,vals):source.append((col,n,field,v))
    low.set_title(f'({"def"[col]}) Deviations from FEM reference',loc='left',fontweight='bold',pad=10,fontsize=8)
    low.set_xticks(x,['FEM\nh = 2','FEM\nh = 1']+['PD\n'+l.replace('delta','δ').replace('dt','Δt').replace(' = ','=') for l in labels],fontsize=6.8)
    low.set_ylabel('Normalised deviation (%)');low.grid(axis='y',color='#dddddd',lw=.5);low.set_axisbelow(True)
handles,labels=axes[1,0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',bbox_to_anchor=(.5,.065),ncol=3,frameon=False,fontsize=7.3)
fig.text(.5,.02,'All comparisons use the same finest 3D FEM reference: h = 2/3 mm; Δt = 25 ns.\nFixed 100 × 20 × 4 mm domain and 2 mm end bands; reference is a numerical approximation.',ha='center',fontsize=7.3)
save(fig,'parameter_sensitivity')
with (R/'parameter_sensitivity.csv').open('w') as f:
    f.write('panel_column,case,metric,deviation_pct\n')
    for row in source:f.write(','.join(map(str,row))+'\n')
print(json.dumps({'metrics':metrics,'time_check':timecheck},indent=2))
