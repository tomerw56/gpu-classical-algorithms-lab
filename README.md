# GPU Classical Algorithms Lab

A C++/CUDA benchmark playground for testing where GPU acceleration helps classical algorithmic workloads: graph theory, constraint checking, cost matrices, spatial events, combinations, local search, and scenario simulation.

This repository starts with **Phase 1: benchmark infrastructure**.

Phase 1 includes:

- CMake build with optional CUDA.
- CPU-only build path.
- Shared benchmark registry.
- Shared benchmark result format.
- JSON Lines output for later plotting.
- Basic CLI runner.
- CPU timer and CUDA timing hooks.
- Deterministic random utilities.
- A small `foundation_smoke` benchmark to verify the infrastructure.

Actual algorithm demos come in later phases.

## Build

### CPU-only

```bash
cmake -S . -B build-cpu -G Ninja -DENABLE_CUDA=OFF
cmake --build build-cpu
```

### CUDA-enabled, when CUDA toolkit is available

```bash
cmake -S . -B build -G Ninja -DENABLE_CUDA=ON
cmake --build build
```

On Windows with Visual Studio generator:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -DENABLE_CUDA=ON
cmake --build build-vs --config Release
```

## Run

List benchmarks:

```bash
./build-cpu/gpu_algobench --list
```

Run the foundation smoke benchmark:

```bash
./build-cpu/gpu_algobench --benchmark foundation_smoke --preset small --repeat 5
```

Write JSONL results:

```bash
./build-cpu/gpu_algobench --benchmark foundation_smoke --preset small --repeat 5 --output results/smoke.jsonl
```

Run all presets using Python:

```bash
python scripts/run_all_benchmarks.py --exe ./build-cpu/gpu_algobench --output results/all.jsonl
```

## Output format

Each benchmark run emits one JSON object per line:

```json
{"benchmark":"foundation_smoke","variant":"cpu","preset":"small","repeat":5,"warmup":1,"input_size":{"values":1000000},"total_ms":3.4,"h2d_ms":0.0,"kernel_ms":0.0,"d2h_ms":0.0,"correct":true,"device":"CPU","notes":"infrastructure smoke benchmark"}
```

Use JSONL because it is easy to append, diff, parse, and plot.

## Phase roadmap

- Phase 1: benchmark infrastructure.
- Phase 2: polynomial, cost matrix, spatial events.
- Phase 3: graph BFS, connected components, weighted relaxation.
- Phase 4: combinations, constraint network, local-search scoring, assignment preprocessing, scenario simulation.

## Important benchmark rule

Always compare:

- CPU total time.
- GPU end-to-end time.
- GPU kernel-only time.
- Host-to-device transfer time.
- Device-to-host transfer time.

The GPU kernel may be fast while the full application is not.
