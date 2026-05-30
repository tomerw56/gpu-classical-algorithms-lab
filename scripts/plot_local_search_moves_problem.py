#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, json
from pathlib import Path

def rows(path):
    with Path(path).open(newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))

def f(r,k): return float(r[k])
def i(r,k): return int(float(r[k]))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--input-dir', required=True)
    ap.add_argument('--output-dir')
    ap.add_argument('--show', action='store_true')
    args=ap.parse_args()
    inp=Path(args.input_dir); out=Path(args.output_dir) if args.output_dir else inp/'plots'; out.mkdir(parents=True, exist_ok=True)
    tasks=rows(inp/'tasks.csv'); res=rows(inp/'resources.csv'); moves=rows(inp/'candidate_moves.csv')
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit(f'matplotlib is required: {exc}')
    paths=[]
    fig,ax=plt.subplots(figsize=(9,7))
    ax.scatter([f(r,'x') for r in res],[f(r,'y') for r in res],marker='^',label='resources')
    ax.scatter([f(t,'x') for t in tasks],[f(t,'y') for t in tasks],marker='o',label='tasks')
    for t in tasks[:40]:
        rid=i(t,'current_resource'); rr=res[rid]
        ax.plot([f(t,'x'),f(rr,'x')],[f(t,'y'),f(rr,'y')],alpha=.25,linewidth=.8)
    ax.set_title('Local-search current assignment geometry'); ax.set_xlabel('x'); ax.set_ylabel('y'); ax.legend(); ax.grid(True,alpha=.3)
    fig.tight_layout(); p=out/'local_search_current_assignment.png'; fig.savefig(p,dpi=150); paths.append(p)
    valid=[m for m in moves if i(m,'valid')==1]
    invalid=[m for m in moves if i(m,'valid')==0]
    improving=[m for m in moves if i(m,'improving')==1]
    fig,ax=plt.subplots(figsize=(9,6))
    ax.hist([f(m,'delta') for m in valid],bins=40,label='valid moves')
    ax.axvline(0.0,linestyle='--',linewidth=1)
    ax.set_title('Candidate move delta distribution'); ax.set_xlabel('delta cost (new - old)'); ax.set_ylabel('count'); ax.legend(); fig.tight_layout(); p=out/'local_search_delta_distribution.png'; fig.savefig(p,dpi=150); paths.append(p)
    fig,ax=plt.subplots(figsize=(7,5))
    ax.bar(['invalid','valid','improving'],[len(invalid),len(valid),len(improving)])
    ax.set_title('Candidate move outcome mix'); ax.set_ylabel('moves'); fig.tight_layout(); p=out/'local_search_move_mix.png'; fig.savefig(p,dpi=150); paths.append(p)
    report=out/'local_search_problem_report.md'
    report.write_text('\n'.join(['# Local-search problem export report','',f'Tasks: {len(tasks)}',f'Resources: {len(res)}',f'Moves: {len(moves)}',f'Valid moves: {len(valid)}',f'Improving moves: {len(improving)}','', 'The geometry plot shows the current assignment; the histogram shows whether candidate replacement moves improve the current objective.']),encoding='utf-8')
    for p in paths: print(f'Wrote plot: {p}')
    print(f'Wrote report: {report}')
    if args.show: plt.show()
if __name__=='__main__': main()
