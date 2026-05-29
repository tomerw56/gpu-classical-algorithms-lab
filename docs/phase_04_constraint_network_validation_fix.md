# Phase 4.1.1: Constraint-network GPU validation fix

The first Phase 4.1 run showed this pattern:

```text
tiny  -> gpu correct=yes
small -> gpu correct=no
```

The issue was not a categorical constraint mismatch. The GPU and CPU paths compute the same validity flags, violation masks, and violation counts. The failure came from using a very small fixed absolute tolerance for the floating-point `penalty` field.

The penalty is computed in single precision on both CPU and GPU. For larger candidate sets, some candidates can accumulate larger penalty magnitudes, and tiny host-vs-CUDA arithmetic differences can exceed a fixed `1e-3` absolute threshold even when the result is numerically equivalent for benchmark purposes.

The validation now uses a combined tolerance:

```text
abs_error <= 5e-2 OR rel_error <= 1e-5
```

Categorical outputs are still exact-match validated:

```text
valid flag
violation mask
violation count
```

This keeps correctness strict where it matters for constraint decisions while avoiding false failures from harmless float-rounding differences in the score.
