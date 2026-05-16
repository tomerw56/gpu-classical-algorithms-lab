# Results Format

The benchmark runner writes JSON Lines, one result per line.

Example:

```json
{"benchmark":"foundation_smoke","variant":"cpu","preset":"small","repeat":5,"warmup":1,"input_size":{"values":1000000},"total_ms":3.4,"h2d_ms":0.0,"kernel_ms":0.0,"d2h_ms":0.0,"correct":true,"device":"CPU","notes":"infrastructure smoke benchmark"}
```

## Fields

- `benchmark`: benchmark name.
- `variant`: `cpu`, `gpu`, or another named variant.
- `preset`: input-size preset.
- `repeat`: number of measured repeats.
- `warmup`: number of warmup runs.
- `input_size`: benchmark-specific dimensions.
- `total_ms`: total measured time.
- `h2d_ms`: host-to-device transfer time.
- `kernel_ms`: GPU kernel time.
- `d2h_ms`: device-to-host transfer time.
- `correct`: correctness result.
- `device`: CPU or GPU description.
- `notes`: short implementation note.

## Why JSONL

JSONL is convenient because it can be appended during long runs, parsed line-by-line, and loaded easily from Python.


## Polynomial metadata

`polynomial_batch` adds benchmark-specific metadata:

- `coefficient_count`: currently `16`.
- `x_step`: currently `100`.
- `x_cycle`: the bounded stride-100 domain cycle length.
- `checksum`: sum of the final output vector.
- `reference_checksum`: CPU-reference sum for the same final vector.
- `max_abs_error`: largest element-wise absolute error.
- `max_rel_error`: largest element-wise relative error.
- `validation`: short description of the validator.

These fields are useful when plotting performance while still preserving evidence that CPU and GPU variants computed the same logical benchmark.


## Cost-matrix metadata

`cost_matrix` adds benchmark-specific metadata:

- `checksum`: sum of feasible final matrix costs.
- `reference_checksum`: CPU-reference sum over feasible final matrix costs.
- `feasible_count`: number of feasible final matrix cells.
- `reference_feasible_count`: CPU-reference feasible-cell count.
- `feasibility_mismatches`: count of GPU/CPU feasibility disagreements.
- `max_abs_error`: largest cell-wise absolute cost error.
- `max_rel_error`: largest cell-wise relative cost error.
- `cost_model`: compact description of the branch-heavy scoring model.
- `validation`: short description of the validator.

These fields make it possible to compare performance while confirming that branch-heavy feasibility logic stayed semantically aligned between CPU and GPU.
