"""Run a supported project demo from a local CMake build."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

REPO=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('demo',choices=['elastic','material-point','tensile','transient','hyperelastic'])
    p.add_argument('--bin-dir',type=Path,default=REPO/'build/release/bin')
    p.add_argument('--threads',type=int,default=1)
    p.add_argument('--resolution',choices=['coarse','fine'],default='coarse')
    p.add_argument('--overwrite',action='store_true',help='Recompute completed benchmark runs.')
    a=p.parse_args()
    if a.threads<1: p.error('threads must be positive')
    bin_dir=a.bin_dir.resolve()
    exe=bin_dir/('vumat_pd_run.exe' if os.name=='nt' else 'vumat_pd_run')
    if not exe.exists(): p.error('Build with cmake --preset release and cmake --build --preset release first.')
    env=dict(os.environ,OMP_NUM_THREADS=str(a.threads),OMP_DYNAMIC='FALSE')
    env['PATH']=str(bin_dir)+os.pathsep+env.get('PATH','')
    def run(cmd): subprocess.run([str(x) for x in cmd],cwd=REPO,env=env,check=True)
    if a.demo=='elastic':
        for case in ['affine_bar_elastic','affine_shear_elastic']: run([exe,REPO/'examples'/f'{case}.json'])
        print('Results: examples/output/affine_bar_elastic and examples/output/affine_shear_elastic')
    elif a.demo=='material-point':
        run([exe,REPO/'examples/jc_material_point.json'])
        print('Results: examples/output/jc_material_point')
    elif a.demo=='hyperelastic':
        suffix='.exe' if os.name=='nt' else ''
        motion=bin_dir/('vumat_pd_large_motion_3d'+suffix)
        check=bin_dir/('neohookean_material_test'+suffix)
        if not motion.exists() or not check.exists(): p.error('Rebuild to include the hyperelastic demo and material check.')
        root=REPO/'demos/output/hyperelastic'
        run([check,REPO/'configs/neohookean.json',root/'material-point-check'])
        for scenario,angle in [('bending',150),('torsion',540)]:
            run([motion,scenario,root/scenario,2400,24,angle])
        print('Results: demos/output/hyperelastic; render with python demos/render_hyperelastic.py')
    else:
        cases=['fem_coarse'] if a.demo=='transient' else (['fem_coarse','pd_coarse'] if a.resolution=='coarse' else ['fem_fine','pd_shared'])
        cmd=[sys.executable,REPO/'benchmarks/tensile3d/run_cases.py','--bin-dir',bin_dir,'--cases',*cases,'--threads',a.threads]
        if a.overwrite: cmd.append('--overwrite')
        if a.demo=='transient': cmd+=['--snapshots',20]
        run(cmd)
        if a.demo=='transient':
            run([sys.executable,REPO/'demos/render_transient.py','--snapshots',REPO/'benchmarks/tensile3d/generated/results/fem_coarse/movie_slice.csv',
                 '--output',REPO/'demos/output/transient.mp4','--gif'])
        else:
            print('Results: benchmarks/tensile3d/generated/results; plot with '
                  f'python demos/plot.py --demo tensile --results generated --resolution {a.resolution}')

if __name__=='__main__': main()
