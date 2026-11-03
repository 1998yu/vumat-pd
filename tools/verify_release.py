"""Run the documented release checks and retain logs; does not commit or publish."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--eigen-dir", type=Path, help="Optional Eigen header root for the independent rotation check")
    args=parser.parse_args()
    output=ROOT/"build/verification"
    output.mkdir(parents=True,exist_ok=True)
    configure=["cmake","--preset","release"]
    if args.eigen_dir:
        configure += ["-DVUMAT_PD_EIGEN_CHECK=ON", f"-DEIGEN3_INCLUDE_DIR={args.eigen_dir.resolve().as_posix()}"]
    else:
        configure += ["-DVUMAT_PD_EIGEN_CHECK=OFF"]
    commands=[
        ("configure",configure),
        ("build",["cmake","--build","--preset","release","--parallel","4"]),
        ("tests",["ctest","--preset","release","--output-on-failure"]),
        ("archive",[sys.executable,"benchmarks/tensile3d/unpack_archive.py"]),
        ("metrics",[sys.executable,"benchmarks/tensile3d/analyse_and_plot.py"]),
        ("compare",[sys.executable,"benchmarks/tensile3d/verify_metrics.py"]),
        ("python",[sys.executable,"-m","compileall","-q","benchmarks/tensile3d","tests","tools"]),
    ]
    report=[]
    for name,command in commands:
        print(f"START {name}",flush=True)
        started=time.monotonic()
        with (output/f"{name}.log").open("w",encoding="utf-8") as log:
            result=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        public_command=["python" if c==sys.executable else c.replace(str(ROOT),"<CHECKOUT>").replace(ROOT.as_posix(),"<CHECKOUT>") for c in command]
        report.append({"step":name,"command":public_command,"returncode":result.returncode,
                       "elapsed_seconds":time.monotonic()-started})
        (output/"execution.json").write_text(json.dumps(report,indent=2)+"\n")
        print(f"END {name}: {result.returncode}",flush=True)
        if result.returncode:
            raise SystemExit(f"Failed {name}; inspect build/verification/{name}.log")

if __name__=="__main__":
    main()
