# Graph weighted-relaxation backend recommendations

This report is generated from the measured sweep results. It does not assume that GPU is always preferable.

## Main conclusion

For this benchmark, CPU Dijkstra remains the best backend for high-diameter chain/grid/layered cases in the measured range. The global GPU relaxation becomes useful on random graphs where enough edge parallelism exists and the iteration count remains low. The frontier, delta-stepping-style, and light/heavy delta-style GPU variants are experimental; the recommendation is based on measured results, not assumptions.

## Recommendation table

| kind | label | nodes | edges | best backend | best ms/run | reason |
|---|---:|---:|---:|---:|---:|---|
| chain | chain_256 | 256 | 510 | cpu | 0.002700 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| chain | chain_1024 | 1024 | 2046 | cpu | 0.011567 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| chain | chain_4096 | 4096 | 8190 | cpu | 0.043300 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| grid | grid_16x16 | 256 | 960 | cpu | 0.007500 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| grid | grid_64x64 | 4096 | 16128 | cpu | 0.305567 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| grid | grid_128x128 | 16384 | 65024 | cpu | 1.397433 | CPU Dijkstra wins: this family has high effective diameter, so GPU relaxation needs many synchronization rounds. |
| layered | layered_8x64_f4 | 512 | 1792 | cpu | 0.004167 | CPU Dijkstra wins: graph is not large/parallel enough to amortize GPU overhead. |
| layered | layered_16x256_f6 | 4096 | 23040 | cpu | 0.066900 | CPU Dijkstra wins: graph is not large/parallel enough to amortize GPU overhead. |
| layered | layered_32x512_f6 | 16384 | 95232 | cpu | 0.295167 | CPU Dijkstra wins: graph is not large/parallel enough to amortize GPU overhead. |
| layered | layered_64x2048_f8 | 131072 | 1032192 | cpu | 2.100533 | CPU Dijkstra wins: GPU relaxation needed many iterations, so repeated global work dominated. |
| random | random_1024_d8 | 1024 | 8192 | cpu | 0.133867 | CPU Dijkstra wins: graph is not large/parallel enough to amortize GPU overhead. |
| random | random_16384_d8 | 16384 | 131072 | gpu-global | 0.837067 | Global GPU scan wins: enough edge parallelism exists and iteration count is low enough that full edge scans are cheaper than CPU Dijkstra. |
| random | random_65536_d8 | 65536 | 524288 | gpu-global | 1.323861 | Global GPU scan wins: enough edge parallelism exists and iteration count is low enough that full edge scans are cheaper than CPU Dijkstra. |
| random | random_1048576_d8 | 1048576 | 8388608 | gpu-delta-stepping | 20.699040 | Delta-stepping-style GPU wins: bucket ordering reduced unnecessary relaxation work enough to beat CPU Dijkstra and the other GPU variants. |

## Next serious GPU direction

The final weighted-graph GPU attempt in this project is `gpu-delta-light-heavy`. If it does not win, the chapter should be treated as an educational counterexample: simple GPU adaptations of weighted SSSP are not enough. Stronger future work would need production-grade delta stepping with per-bucket deduplication and better device-side scheduling, or structure-aware backends: prefix/scan-style treatment for chain-like graphs, tiled/local methods for grids, and global GPU scans for low-diameter random graphs.