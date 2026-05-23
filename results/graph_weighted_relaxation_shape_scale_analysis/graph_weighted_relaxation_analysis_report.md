# Graph Weighted Relaxation JSONL Analysis

Rows read: 70
Matched CPU/GPU size points: 14
Invalid global scan GPU rows: 0
Invalid frontier GPU rows: 0
Invalid delta-stepping GPU rows: 0
Invalid delta light/heavy GPU rows: 0

## First GPU crossover by family

- `chain` / global scan: no GPU crossover observed in this sweep.
- `chain` / frontier: no GPU crossover observed in this sweep.
- `chain` / delta-stepping: no GPU crossover observed in this sweep.
- `chain` / delta light/heavy: no GPU crossover observed in this sweep.
- `grid` / global scan: no GPU crossover observed in this sweep.
- `grid` / frontier: no GPU crossover observed in this sweep.
- `grid` / delta-stepping: no GPU crossover observed in this sweep.
- `grid` / delta light/heavy: no GPU crossover observed in this sweep.
- `layered` / global scan: no GPU crossover observed in this sweep.
- `layered` / frontier: no GPU crossover observed in this sweep.
- `layered` / delta-stepping: no GPU crossover observed in this sweep.
- `layered` / delta light/heavy: no GPU crossover observed in this sweep.
- `random` / global scan: first crossover at `random_16384_d8` (16384 nodes, 131072 edges), speedup=3.537x.
- `random` / frontier: first crossover at `random_16384_d8` (16384 nodes, 131072 edges), speedup=2.227x.
- `random` / delta-stepping: first crossover at `random_65536_d8` (65536 nodes, 524288 edges), speedup=3.383x.
- `random` / delta light/heavy: first crossover at `random_65536_d8` (65536 nodes, 524288 edges), speedup=3.616x.

## Summary table

| kind | label | nodes | edges | CPU | global | frontier | delta | light/heavy | CPU/global | CPU/frontier | CPU/delta | CPU/light-heavy | iters global/frontier/delta/light-heavy |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| chain | chain_256 | 256 | 510 | 0.002700 | 9.491338 | 16.249535 | 45.878463 | 59.123540 | 0.000284 | 0.000166 | 0.000059 | 0.000046 | 256/256/256/256 |
| chain | chain_1024 | 1024 | 2046 | 0.011567 | 37.372054 | 63.359455 | 195.105182 | 263.746001 | 0.000310 | 0.000183 | 0.000059 | 0.000044 | 1024/1024/1024/1024 |
| chain | chain_4096 | 4096 | 8190 | 0.043300 | 154.374927 | 267.178251 | 765.892918 | 2021.349064 | 0.000280 | 0.000162 | 0.000057 | 0.000021 | 4096/4096/4096/4096 |
| grid | grid_16x16 | 256 | 960 | 0.007500 | 1.202880 | 2.492437 | 7.787542 | 8.001142 | 0.006235 | 0.003009 | 0.000963 | 0.000937 | 31/31/50/50 |
| grid | grid_64x64 | 4096 | 16128 | 0.305567 | 4.750741 | 8.132949 | 31.990283 | 36.896309 | 0.064320 | 0.037571 | 0.009552 | 0.008282 | 128/128/242/242 |
| grid | grid_128x128 | 16384 | 65024 | 1.397433 | 10.178805 | 16.484022 | 64.124546 | 78.225022 | 0.137289 | 0.084775 | 0.021792 | 0.017864 | 258/259/533/533 |
| layered | layered_8x64_f4 | 512 | 1792 | 0.004167 | 0.357867 | 0.693269 | 2.158539 | 2.718283 | 0.011643 | 0.006010 | 0.001930 | 0.001533 | 8/8/16/16 |
| layered | layered_16x256_f6 | 4096 | 23040 | 0.066900 | 0.567115 | 0.930933 | 5.352800 | 8.530272 | 0.117966 | 0.071863 | 0.012498 | 0.007843 | 16/16/49/49 |
| layered | layered_32x512_f6 | 16384 | 95232 | 0.295167 | 1.400267 | 1.883744 | 14.498581 | 18.728533 | 0.210793 | 0.156691 | 0.020358 | 0.015760 | 32/32/117/117 |
| layered | layered_64x2048_f8 | 131072 | 1032192 | 2.100533 | 3.783509 | 4.356000 | 31.791519 | 40.495083 | 0.555181 | 0.482216 | 0.066072 | 0.051871 | 50/64/285/285 |
| random | random_1024_d8 | 1024 | 8192 | 0.133867 | 0.612843 | 0.831061 | 2.201835 | 2.270912 | 0.218436 | 0.161079 | 0.060798 | 0.058948 | 14/14/19/19 |
| random | random_16384_d8 | 16384 | 131072 | 2.961033 | 0.837067 | 1.329856 | 3.197749 | 3.816971 | 3.537392 | 2.226582 | 0.925974 | 0.775755 | 13/18/26/26 |
| random | random_65536_d8 | 65536 | 524288 | 14.408667 | 1.323861 | 1.916608 | 4.258784 | 3.985205 | 10.883819 | 7.517795 | 3.383282 | 3.615539 | 12/20/32/32 |
| random | random_1048576_d8 | 1048576 | 8388608 | 358.767000 | 23.259808 | 26.346304 | 20.699040 | 24.281601 | 15.424332 | 13.617356 | 17.332543 | 14.775261 | 17/27/42/42 |

## Interpretation

The CPU baseline is Dijkstra with a priority queue over positive integer weights.
The original `gpu` variant is a global Bellman-Ford-style edge scan: every pass scans all edges.
`gpu-frontier` and `gpu-delta-stepping` are active-set/bucket experiments.
`gpu-delta-light-heavy` is the final weighted-graph attempt in this project: it closes a bucket using light edges before relaxing heavy edges out of that bucket.
If the light/heavy variant still does not win, the weighted SSSP chapter should be treated as a valuable counterexample: simple GPU adaptations are not enough to beat a strong CPU Dijkstra baseline except for low-diameter random graphs where the global GPU scan has enough parallel work.