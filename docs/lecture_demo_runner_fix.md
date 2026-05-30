# Lecture demo runner fix

`execute_lecture_demo_core.bat` originally used a batch subroutine pattern like:

```bat
call :run foundation_smoke --benchmark foundation_smoke --preset small
...
shift /1
"%GPU_ALGOBENCH%" %*
```

That is unsafe in `cmd.exe` because `%*` expands to the original subroutine argument list. The demo label (`foundation_smoke`, `polynomial_batch`, etc.) was still passed to `gpu_algobench.exe`, causing the executable to print usage/help for every demo instead of running the benchmark.

The fixed runner passes the benchmark command as a quoted command fragment:

```bat
call :run foundation_smoke "--benchmark foundation_smoke --preset small"
```

and the subroutine executes `%~2` as the command fragment. It also echoes the full command before running it, making future issues easier to diagnose.
