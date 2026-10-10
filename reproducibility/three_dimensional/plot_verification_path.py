from pathlib import Path
import sys
R=Path(__file__).resolve().parent
R=R/'generated'
R.mkdir(exist_ok=True)
import matplotlib as mpl
mpl.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
mpl.rcParams.update({'font.family':'sans-serif','font.sans-serif':['Arial','DejaVu Sans'],'svg.fonttype':'none','pdf.fonttype':42})
fig,ax=plt.subplots(figsize=(183/25.4,61/25.4));fig.subplots_adjust(left=.01,right=.99,bottom=.04,top=.9)
ax.set(xlim=(0,1),ylim=(0,1));ax.axis('off')
def box(x,y,w,h,title,body,colour):
    ax.add_patch(Rectangle((x,y),w,h,facecolor='#F3F6F8',edgecolor=colour,lw=1))
    ax.text(x+w/2,y+h-.055,title,ha='center',va='top',fontsize=8,fontweight='bold',color='#253A49')
    ax.text(x+w/2,y+.035,body,ha='center',va='bottom',fontsize=7.1,linespacing=1.25)
box(.015,.33,.19,.34,'Common inputs','3D geometry + bands\nLoading + JC properties','#6A8CA5')
box(.265,.33,.23,.34,'Shared material transfer','Same compiled JC VUMAT\nSame adapter and history','#C77E3D')
box(.56,.63,.19,.33,'3D FEM path','H8 Gauss-point kinematics\nIndependent FEM forces','#286391')
box(.56,.04,.19,.33,'3D PD path','Nonlocal F reconstruction\nIndependent PD forces','#6A8CA5')
box(.81,.33,.18,.34,'Common reference','Finest 3D H8 FEM\nStress + displacement\nMaximum plastic strain','#286391')
def arrow(a,b):ax.add_patch(FancyArrowPatch(a,b,arrowstyle='-|>',mutation_scale=10,lw=1,color='#3B4B57'))
for a,b in [((.205,.5),(.265,.5)),((.495,.5),(.56,.795)),((.495,.5),(.56,.205)),((.75,.795),(.81,.5)),((.75,.205),(.81,.5))]:arrow(a,b)
fig.suptitle('Verification with matched three-dimensional models',fontsize=10,fontweight='bold',y=.99)
o=R/'figures';o.mkdir(exist_ok=True)
for ext in ['svg','pdf','png','tiff']:fig.savefig(o/f'Figure3_3D.{ext}',dpi=600,facecolor='white')
fig.savefig(o/'Figure3_3D_preview.png',dpi=160,facecolor='white')
