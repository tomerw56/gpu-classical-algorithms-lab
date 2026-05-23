# Graph Connected Components JSONL Analysis

Rows read: 48
Matched CPU/GPU size points: 16
Invalid GPU rows: 0
Unconverged GPU rows: 0

## First naive-GPU crossover by family

- `chains`: no crossover observed in this sweep.
- `grid_components`: first crossover at `grid_16x64x64` (65536 nodes, 258048 edges), speedup=1.930x.
- `random_clusters`: first crossover at `random_32x512_d4` (16384 nodes, 161732 edges), speedup=2.493x.
- `mixed`: no crossover observed in this sweep.

## First non-naive-GPU crossover by family

- `chains`: first crossover at `chains_64x512` (32768 nodes, 65408 edges), speedup=1.589x.
- `grid_components`: first crossover at `grid_8x32x32` (8192 nodes, 31744 edges), speedup=1.834x.
- `random_clusters`: first crossover at `random_16x256_d4` (4096 nodes, 40020 edges), speedup=1.081x.
- `mixed`: first crossover at `mixed_32x512` (16384 nodes, 42762 edges), speedup=1.133x.

## Summary table

| kind | label | nodes | edges | CPU ms/run | GPU naive ms/run | GPU non-naive ms/run | speedup naive | speedup non-naive | naive/non-naive | it naive | it non-naive |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| chains | chains_8x64 | 512 | 1008 | 0.021733 | 0.350453 | 0.131915 | 0.062015 | 0.164753 | 2.656667 | 7 | 2 |
| chains | chains_16x128 | 2048 | 4064 | 0.019000 | 0.479531 | 0.123552 | 0.039622 | 0.153781 | 3.881205 | 8 | 2 |
| chains | chains_32x256 | 8192 | 16320 | 0.117333 | 0.683659 | 0.152405 | 0.171626 | 0.769877 | 4.485792 | 9 | 2 |
| chains | chains_64x512 | 32768 | 65408 | 0.354400 | 0.536907 | 0.223019 | 0.660077 | 1.589105 | 2.407452 | 10 | 2 |
| grid_components | grid_4x16x16 | 1024 | 3840 | 0.010733 | 0.278251 | 0.133227 | 0.038574 | 0.080564 | 2.088551 | 5 | 2 |
| grid_components | grid_8x32x32 | 8192 | 31744 | 0.332500 | 0.373536 | 0.181259 | 0.890142 | 1.834395 | 2.060790 | 6 | 2 |
| grid_components | grid_16x64x64 | 65536 | 258048 | 0.967733 | 0.501355 | 0.317269 | 1.930237 | 3.050195 | 1.580218 | 7 | 2 |
| grid_components | grid_24x96x96 | 221184 | 875520 | 3.556500 | 0.806187 | 0.731712 | 4.411509 | 4.860519 | 1.101781 | 7 | 2 |
| random_clusters | random_8x128_d4 | 1024 | 9844 | 0.031800 | 0.144320 | 0.264896 | 0.220344 | 0.120047 | 0.544818 | 2 | 2 |
| random_clusters | random_16x256_d4 | 4096 | 40020 | 0.162933 | 0.170656 | 0.150688 | 0.954747 | 1.081263 | 1.132512 | 2 | 2 |
| random_clusters | random_32x512_d4 | 16384 | 161732 | 0.591333 | 0.237227 | 0.226240 | 2.492693 | 2.613744 | 1.048562 | 3 | 2 |
| random_clusters | random_64x1024_d4 | 65536 | 651488 | 2.460733 | 0.361451 | 0.521483 | 6.807937 | 4.718725 | 0.693121 | 3 | 2 |
| mixed | mixed_8x128 | 1024 | 2510 | 0.008333 | 0.487211 | 0.142165 | 0.017104 | 0.058617 | 3.427071 | 8 | 2 |
| mixed | mixed_16x256 | 4096 | 10630 | 0.054433 | 2.165003 | 0.145792 | 0.025142 | 0.373363 | 14.849941 | 9 | 2 |
| mixed | mixed_32x512 | 16384 | 42762 | 0.206867 | 1.043211 | 0.182517 | 0.198298 | 1.133408 | 5.715680 | 9 | 2 |
| mixed | mixed_64x1024 | 65536 | 173574 | 0.749767 | 1.953632 | 0.329973 | 0.383781 | 2.272204 | 5.920575 | 11 | 2 |

## Interpretation

The CPU baseline is Union-Find. The `gpu` row is the original node-parallel label-propagation baseline.
The `gpu-non-naive` row uses edge-parallel root hooking plus full pointer-jumping compression.
The new row is intended to show whether fewer convergence rounds can compensate for the extra work done inside each GPU iteration.
