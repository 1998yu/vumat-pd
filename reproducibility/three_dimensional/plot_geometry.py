from pathlib import Path
import sys
R=Path(__file__).resolve().parent
R=R/'generated'
R.mkdir(exist_ok=True)
import numpy as np
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle
mpl.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Arial','DejaVu Sans'],'font.size':8,'pdf.fonttype':42,'svg.fonttype':'none'})
fig,(ax,load)=plt.subplots(2,1,figsize=(183/25.4,106/25.4),gridspec_kw={'height_ratios':[1.35,1]})
fig.subplots_adjust(left=.12,right=.93,top=.94,bottom=.12,hspace=.46)
ax.add_patch(Rectangle((0,-10),100,20,facecolor='#E6EFF4',edgecolor='#3B4B57',lw=1))
ax.add_patch(Polygon([[0,10],[5,15],[105,15],[100,10]],facecolor='#F0F4F7',edgecolor='#3B4B57',lw=.8))
ax.add_patch(Polygon([[100,-10],[105,-5],[105,15],[100,10]],facecolor='#CBD9E3',edgecolor='#3B4B57',lw=.8))
for x,c in [(0,'#286391'),(98,'#C77E3D')]:ax.add_patch(Rectangle((x,-10),2,20,facecolor=c,alpha=.4,edgecolor=c,lw=.6))
ax.annotate('',xy=(100,19),xytext=(0,19),arrowprops={'arrowstyle':'|-|','lw':.8,'color':'#3B4B57'})
ax.text(50,20.5,'100 mm',ha='center',fontsize=8)
ax.annotate('',xy=(-5,10),xytext=(-5,-10),arrowprops={'arrowstyle':'|-|','lw':.8,'color':'#3B4B57'})
ax.text(-8,0,'20 mm',rotation=90,ha='center',va='center')
ax.text(108,16,'4 mm\nthickness',ha='left',va='center',fontsize=7.8)
ax.plot([2,2],[0,10],marker='o',ls='none',color='#286391',ms=3)
ax.text(51,1,'3D tensile specimen\nFree lateral and thickness faces',ha='center',va='center',fontsize=9)
for y in [-7,0,7]:ax.annotate('',xy=(113,y),xytext=(103,y),arrowprops={'arrowstyle':'->','color':'#C77E3D','lw':1.3})
ax.text(108,-12,'uₓ(t)',color='#C77E3D',ha='center')
ax.text(0,-16,'Support band: 0 ≤ x ≤ 2 mm\nuₓ = 0',color='#286391',fontsize=7.6,va='top')
ax.text(100,-16,'Loaded band: 98 ≤ x ≤ 100 mm\nuₓ = uₓ(t)',color='#C77E3D',fontsize=7.6,ha='right',va='top')
ax.text(50,-25,'Rigid-body pins: uᵧ = u_z = 0 at (2, 0, 0); u_z = 0 at (2, 10, 0) mm.\nThe same fixed domain and bands are used for every FEM and PD grid.',fontsize=7.6,ha='center',va='top')
ax.set(xlim=(-11,119),ylim=(-34,26));ax.axis('off');ax.set_title('(a) Common three-dimensional geometry and constraints',loc='left',fontsize=9,fontweight='bold',pad=9)
t=np.linspace(0,.12,241);u=1-np.cos(np.pi*t/.12)
load.plot(t,u,color='#C77E3D',lw=1.8);load.set(xlim=(0,.12),ylim=(0,2.15),xlabel='Time (ms)',ylabel='End displacement (mm)')
load.text(.008,1.65,r'$u_x(t)=\frac{U}{2}[1-\cos(\pi t/T)]$',fontsize=10)
load.text(.07,.3,'U = 2 mm; T = 0.12 ms',color='#C77E3D',fontsize=8)
load.set_xticks([0,.04,.08,.12]);load.spines[['top','right']].set_visible(False)
load.set_title('(b) Prescribed loading history',loc='left',fontsize=9,fontweight='bold',pad=8)
o=R/'figures';o.mkdir(exist_ok=True)
for ext in ['svg','pdf','png','tiff']:fig.savefig(o/f'Figure2_3D.{ext}',dpi=600,facecolor='white')
fig.savefig(o/'Figure2_3D_preview.png',dpi=160,facecolor='white')
