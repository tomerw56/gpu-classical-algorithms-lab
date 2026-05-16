# CUDA Runtime Diagnostics

This project can compile CUDA code and still fail to use the GPU at runtime. That usually means one of these is true:

- The NVIDIA driver is too old for the CUDA Runtime version used by the build.
- No NVIDIA GPU is visible to the process.
- The process is running in a remote/container/WSL environment where the GPU is not exposed.
- CUDA was disabled at build time.

Run:

```powershell
.\build-cuda\Release\cuda_probe.exe
```

Also compare with:

```powershell
nvidia-smi
nvcc --version
```

The benchmark output also writes a `cuda_status` metadata field in JSONL output.
