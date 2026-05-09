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
