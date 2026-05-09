# GPU Pitfalls

## GPU is throughput hardware

A GPU is not a faster CPU. It works best when thousands or millions of similar operations can run in parallel.

Good GPU shape:

```text
many independent items
flat arrays
same operation per item
compact output
data stays on GPU across steps
```

Poor GPU shape:

```text
small input
one global priority queue
deep branching
pointer-heavy structures
recursive logic
large CPU/GPU roundtrips
huge output materialization
```

## Common benchmark traps

### Measuring only kernel time

Kernel-only time is useful, but hardware decisions should use end-to-end time.

### Copying too often

Bad pattern:

```text
CPU -> GPU
small kernel
GPU -> CPU
CPU logic
CPU -> GPU
small kernel
GPU -> CPU
```

Better pattern:

```text
copy big input once
run many kernels
copy compact result back
```

### Branch divergence

Branch-heavy code can make neighboring GPU threads follow different paths. This reduces efficiency.

### Atomics

Atomics are sometimes necessary, especially in graph algorithms. They can also create contention.

### Output explosion

Combination search and spatial matching can produce enormous outputs. Often the GPU should return counts, best candidates, histograms, or top-K results rather than every match.
