#!/usr/bin/env python3
from __future__ import annotations

import argparse, csv, json
from collections import defaultdict
from pathlib import Path


def load_summary(path: Path):
    if path.suffix.lower()=='.csv':
        with path.open(newline='', encoding='utf-8') as f:
            return list(csv.DictReader(f))
    # Accept JSONL directly by shelling through same logic? Keep simple fallback.
    raise SystemExit('Please pass the analysis CSV produced by analyze_local_search_moves_jsonl.py')

def f(row,key,default=0.0):
    v=row.get(key,'')
    if v in ('', 'None', 'n/a'): return default
    return float(v)

def i(row,key,default=0):
    v=row.get(key,'')
    if v in ('', 'None', 'n/a'): return default
    return int(float(v))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--summary-csv', required=True)
    ap.add_argument('--output-dir', default='results/local_search_moves_plots')
    ap.add_argument('--show', action='store_true')
    args=ap.parse_args()
    out=Path(args.output_dir); out.mkdir(parents=True, exist_ok=True)
    rows=load_summary(Path(args.summary_csv))
    rows.sort(key=lambda r:i(r,'moves'))
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit(f'matplotlib is required: {exc}')
    paths=[]
    # times
    fig,ax=plt.subplots(figsize=(10,6))
    ax.plot([i(r,'moves') for r in rows],[f(r,'cpu_ms_per_run') for r in rows],marker='o',label='CPU')
    ax.plot([i(r,'moves') for r in rows],[f(r,'gpu_ms_per_run') for r in rows],marker='s',linestyle='--',label='GPU total')
    ax.plot([i(r,'moves') for r in rows],[f(r,'gpu_kernel_ms_per_run') for r in rows],marker='^',linestyle=':',label='GPU kernel')
    ax.set_xscale('log'); ax.set_yscale('log'); ax.grid(True,which='both',alpha=.3)
    ax.set_xlabel('candidate moves'); ax.set_ylabel('ms per run'); ax.set_title('Local-search move evaluation timing')
    ax.legend(); fig.tight_layout(); p=out/'local_search_moves_times.png'; fig.savefig(p,dpi=150); paths.append(p)
    # speedup
    fig,ax=plt.subplots(figsize=(10,6))
    ax.plot([i(r,'moves') for r in rows],[f(r,'speedup_cpu_over_gpu') for r in rows],marker='o')
    ax.axhline(1.0,linestyle='--',linewidth=1); ax.set_xscale('log'); ax.set_yscale('log'); ax.grid(True,which='both',alpha=.3)
    ax.set_xlabel('candidate moves'); ax.set_ylabel('CPU/GPU speedup'); ax.set_title('Local-search move evaluation speedup')
    fig.tight_layout(); p=out/'local_search_moves_speedup.png'; fig.savefig(p,dpi=150); paths.append(p)
    # ratios
    fig,ax=plt.subplots(figsize=(10,6))
    moves=[i(r,'moves') for r in rows]
    valid=[i(r,'valid_moves')/max(1,i(r,'moves')) for r in rows]
    improving=[i(r,'improving_moves')/max(1,i(r,'moves')) for r in rows]
    ax.plot(moves,valid,marker='o',label='valid ratio')
    ax.plot(moves,improving,marker='s',label='improving ratio')
    ax.set_xscale('log'); ax.grid(True,which='both',alpha=.3); ax.set_ylim(bottom=0)
    ax.set_xlabel('candidate moves'); ax.set_ylabel('ratio'); ax.set_title('Valid and improving move ratios')
    ax.legend(); fig.tight_layout(); p=out/'local_search_moves_valid_improving_ratio.png'; fig.savefig(p,dpi=150); paths.append(p)
    # throughput: when this flattens, speedup also flattens
    fig,ax=plt.subplots(figsize=(10,6))
    cpu_throughput=[i(r,'moves')/max(1e-12,f(r,'cpu_ms_per_run')) for r in rows]
    gpu_throughput=[i(r,'moves')/max(1e-12,f(r,'gpu_ms_per_run')) for r in rows]
    gpu_kernel_throughput=[i(r,'moves')/max(1e-12,f(r,'gpu_kernel_ms_per_run')) for r in rows]
    ax.plot(moves,cpu_throughput,marker='o',label='CPU total')
    ax.plot(moves,gpu_throughput,marker='s',linestyle='--',label='GPU total')
    ax.plot(moves,gpu_kernel_throughput,marker='^',linestyle=':',label='GPU kernel only')
    ax.set_xscale('log'); ax.set_yscale('log'); ax.grid(True,which='both',alpha=.3)
    ax.set_xlabel('candidate moves'); ax.set_ylabel('moves per millisecond')
    ax.set_title('Local-search move throughput')
    ax.legend(); fig.tight_layout(); p=out/'local_search_moves_throughput.png'; fig.savefig(p,dpi=150); paths.append(p)

    # GPU overhead: total time minus kernel time. This includes copies, allocation/setup, and reductions.
    fig,ax=plt.subplots(figsize=(10,6))
    kernel=[f(r,'gpu_kernel_ms_per_run') for r in rows]
    overhead=[max(0.0,f(r,'gpu_ms_per_run')-f(r,'gpu_kernel_ms_per_run')) for r in rows]
    x=[str(i(r,'moves')) for r in rows]
    ax.bar(x,kernel,label='GPU kernel')
    ax.bar(x,overhead,bottom=kernel,label='GPU non-kernel overhead')
    ax.set_yscale('log'); ax.set_xlabel('candidate moves'); ax.set_ylabel('ms per run')
    ax.set_title('GPU total time breakdown')
    ax.tick_params(axis='x', rotation=45); ax.legend(); fig.tight_layout(); p=out/'local_search_moves_gpu_breakdown.png'; fig.savefig(p,dpi=150); paths.append(p)

    # violation stacked counts
    labels=['skill','capacity','distance','risk','no-change']
    fig,ax=plt.subplots(figsize=(10,6))
    bottoms=[0]*len(rows)
    for key,label in [('skill_violations','skill'),('capacity_violations','capacity'),('distance_violations','distance'),('risk_violations','risk'),('no_change_violations','no-change')]:
        vals=[i(r,key) for r in rows]
        ax.bar([str(i(r,'moves')) for r in rows], vals, bottom=bottoms, label=label)
        bottoms=[a+b for a,b in zip(bottoms,vals)]
    ax.set_xlabel('candidate moves'); ax.set_ylabel('violation count'); ax.set_title('Local-search move rejection reasons')
    ax.tick_params(axis='x', rotation=45); ax.legend(); fig.tight_layout(); p=out/'local_search_moves_violation_reasons.png'; fig.savefig(p,dpi=150); paths.append(p)

    # compact Markdown note that explains plateau detection from the observed rows.
    plateau_path=out/'local_search_moves_speedup_plateau_report.md'
    best_speedup=max([f(r,'speedup_cpu_over_gpu') for r in rows] or [0.0])
    last_speedup=f(rows[-1],'speedup_cpu_over_gpu') if rows else 0.0
    last_gpu_tp=gpu_throughput[-1] if gpu_throughput else 0.0
    prev_gpu_tp=gpu_throughput[-2] if len(gpu_throughput)>1 else 0.0
    lines=[
        '# Local-search speedup plateau note',
        '',
        f'Best observed speedup: {best_speedup:.3f}x.',
        f'Last observed speedup: {last_speedup:.3f}x.',
        '',
        'The speedup rises while fixed GPU launch/copy/reduction overhead is being amortized.',
        'It flattens once CPU and GPU both scale approximately linearly with the number of candidate moves.',
        '',
        f'Last GPU total throughput: {last_gpu_tp:,.0f} moves/ms.',
        f'Previous GPU total throughput: {prev_gpu_tp:,.0f} moves/ms.',
        '',
        'At that point, adding more independent moves increases both CPU time and GPU time by similar factors, so the ratio stops growing.',
        'The plateau is therefore expected for this kernel unless each move becomes more expensive, more data is kept resident on the GPU, or the reduction/output path is optimized further.',
    ]
    plateau_path.write_text('\n'.join(lines), encoding='utf-8')
    paths.append(plateau_path)
    for p in paths: print(f'Wrote plot: {p}')
    if args.show: plt.show()

if __name__=='__main__':
    main()
