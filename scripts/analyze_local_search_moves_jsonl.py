#!/usr/bin/env python3
from __future__ import annotations

import argparse, csv, json
from pathlib import Path
from collections import defaultdict


def load_rows(path: Path):
    rows=[]
    for line_no, raw in enumerate(path.read_text(encoding='utf-8').splitlines(), start=1):
        if not raw.strip():
            continue
        obj=json.loads(raw)
        if obj.get('benchmark')!='local_search_moves':
            continue
        meta=obj.get('metadata') or {}
        inp=obj.get('input_size') or {}
        repeat=int(obj.get('repeat',1)) or 1
        rows.append({
            'variant': obj.get('variant',''),
            'label': obj.get('preset',''),
            'tasks': int(inp.get('tasks',0)),
            'resources': int(inp.get('resources',0)),
            'moves': int(inp.get('moves',0)),
            'ms_per_run': float(obj.get('total_ms',0.0))/repeat,
            'kernel_ms_per_run': float(obj.get('kernel_ms',0.0))/repeat,
            'correct': bool(obj.get('correct',False)),
            'valid_moves': int(meta.get('valid_moves',0) or 0),
            'invalid_moves': int(meta.get('invalid_moves',0) or 0),
            'improving_moves': int(meta.get('improving_moves',0) or 0),
            'skill_violations': int(meta.get('skill_violations',0) or 0),
            'capacity_violations': int(meta.get('capacity_violations',0) or 0),
            'distance_violations': int(meta.get('distance_violations',0) or 0),
            'risk_violations': int(meta.get('risk_violations',0) or 0),
            'no_change_violations': int(meta.get('no_change_violations',0) or 0),
            'best_delta': float(meta.get('best_delta',0.0) or 0.0),
            'best_move_index': int(meta.get('best_move_index',-1) or -1),
            'max_abs_error': float(meta.get('max_abs_error',0.0) or 0.0),
        })
    return rows


def summarize(rows):
    groups=defaultdict(dict)
    for r in rows:
        groups[(r['label'],r['tasks'],r['resources'],r['moves'])][r['variant']]=r
    out=[]
    for (label,tasks,resources,moves), g in groups.items():
        cpu=g.get('cpu')
        gpu=g.get('gpu')
        speedup=None
        if cpu and gpu and gpu['ms_per_run']>0:
            speedup=cpu['ms_per_run']/gpu['ms_per_run']
        rep=gpu or cpu
        out.append({
            'label': label, 'tasks': tasks, 'resources': resources, 'moves': moves,
            'cpu_ms_per_run': cpu['ms_per_run'] if cpu else None,
            'gpu_ms_per_run': gpu['ms_per_run'] if gpu else None,
            'gpu_kernel_ms_per_run': gpu['kernel_ms_per_run'] if gpu else None,
            'speedup_cpu_over_gpu': speedup,
            'valid_moves': rep['valid_moves'] if rep else 0,
            'invalid_moves': rep['invalid_moves'] if rep else 0,
            'improving_moves': rep['improving_moves'] if rep else 0,
            'best_delta': rep['best_delta'] if rep else 0,
            'skill_violations': rep['skill_violations'] if rep else 0,
            'capacity_violations': rep['capacity_violations'] if rep else 0,
            'distance_violations': rep['distance_violations'] if rep else 0,
            'risk_violations': rep['risk_violations'] if rep else 0,
            'no_change_violations': rep['no_change_violations'] if rep else 0,
            'cpu_correct': cpu['correct'] if cpu else False,
            'gpu_correct': gpu['correct'] if gpu else False,
        })
    return sorted(out, key=lambda r:r['moves'])


def fmt(v):
    if v is None: return 'n/a'
    if isinstance(v,float): return f'{v:.6f}'
    return str(v)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--jsonl', required=True)
    ap.add_argument('--output-dir', default='results/local_search_moves_analysis')
    args=ap.parse_args()
    outdir=Path(args.output_dir); outdir.mkdir(parents=True, exist_ok=True)
    rows=summarize(load_rows(Path(args.jsonl)))
    csv_path=outdir/'local_search_moves_analysis_summary.csv'
    fields=['label','tasks','resources','moves','cpu_ms_per_run','gpu_ms_per_run','gpu_kernel_ms_per_run','speedup_cpu_over_gpu','valid_moves','invalid_moves','improving_moves','best_delta','skill_violations','capacity_violations','distance_violations','risk_violations','no_change_violations','cpu_correct','gpu_correct']
    with csv_path.open('w', newline='', encoding='utf-8') as f:
        w=csv.DictWriter(f, fields); w.writeheader(); w.writerows(rows)
    report=outdir/'local_search_moves_analysis_report.md'
    lines=['# Local Search Moves JSONL Analysis','',f'Matched CPU/GPU size points: {len(rows)}','','## Summary table','','| label | moves | CPU ms/run | GPU ms/run | speedup | valid | improving | best delta |','|---|---:|---:|---:|---:|---:|---:|---:|']
    for r in rows:
        lines.append(f"| {r['label']} | {r['moves']} | {fmt(r['cpu_ms_per_run'])} | {fmt(r['gpu_ms_per_run'])} | {fmt(r['speedup_cpu_over_gpu'])} | {r['valid_moves']} | {r['improving_moves']} | {fmt(r['best_delta'])} |")
    lines += ['', '## Interpretation', '', 'The GPU version evaluates candidate moves independently. It should improve as the number of moves grows, but very small move sets can remain CPU-favored because launch and transfer overhead dominate.']
    report.write_text('\n'.join(lines), encoding='utf-8')
    print(f'Wrote summary CSV: {csv_path}')
    print(f'Wrote report: {report}')

if __name__=='__main__':
    main()
