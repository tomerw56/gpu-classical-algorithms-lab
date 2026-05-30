@echo off
setlocal EnableExtensions

REM ============================================================
REM Lecture core demo runner
REM
REM This is not the full benchmark suite. It runs a curated set of quick
REM demonstrations that support the lecture narrative:
REM   1. dense independent work is GPU-friendly,
REM   2. graph anatomy matters,
REM   3. optimization-support workloads are strong hybrid CPU/GPU candidates.
REM
REM For rigorous evidence, use the per-phase all-sweeps runners.
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "RESULTS_DIR=results"
set "DEMO_RESULTS_FILE=lecture_demo_core.jsonl"
set "DEMO_REPEAT=3"
set "DEMO_WARMUP=1"
set "RUN_WEIGHTED_DEMO=1"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "LIST_BENCHMARKS=%EXE_DIR%\list_benchmarks.exe"
set "DEMO_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%DEMO_RESULTS_FILE%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found:
    echo   %GPU_ALGOBENCH%
    echo Build first:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
if exist "%DEMO_JSONL%" del "%DEMO_JSONL%"

echo.
echo ============================================================
echo LECTURE CORE DEMO
echo ============================================================
echo Results:
echo   %DEMO_JSONL%
echo.

if exist "%LIST_BENCHMARKS%" (
    echo --- Available benchmarks ---
    "%LIST_BENCHMARKS%"
)

call :run foundation_smoke "--benchmark foundation_smoke --preset small"
call :run polynomial_batch "--benchmark polynomial_batch --preset medium"
call :run cost_matrix "--benchmark cost_matrix --preset small"
call :run spatial_events "--benchmark spatial_events --preset small"

REM Graph counterexample and graph anatomy demos.
call :run graph_bfs_chain "--benchmark graph_bfs --preset tiny --set graph=chain --set chain_nodes=4096 --set sweep_label=lecture_bfs_chain_4096"
call :run graph_bfs_random "--benchmark graph_bfs --preset tiny --set graph=random --set random_nodes=16384 --set random_out_degree=8 --set sweep_label=lecture_bfs_random_16k"
call :run graph_connected_components "--benchmark graph_connected_components --preset tiny --set graph=mixed --set components=32 --set component_size=512 --set sweep_label=lecture_cc_mixed_32x512"

if "%RUN_WEIGHTED_DEMO%"=="1" (
    REM Weighted shortest path is an educational counterexample; this row can be slower.
    call :run graph_weighted_random "--benchmark graph_weighted_relaxation --preset tiny --set graph=random --set random_nodes=16384 --set random_out_degree=8 --set sweep_label=lecture_weighted_random_16k"
)

REM Optimization-support demos.
call :run constraint_network "--benchmark constraint_network --preset small"
call :run combination_finder "--benchmark combination_finder --preset small"
call :run assignment_preprocessing "--benchmark assignment_preprocessing --preset medium"
call :run local_search_moves "--benchmark local_search_moves --preset tiny --set sweep_label=lecture_local_search_1m --set tasks=2048 --set resources=2048 --set moves=1048576"
call :run scenario_simulation "--benchmark scenario_simulation --preset tiny --set sweep_label=lecture_scenario_64k --set tasks=128 --set resources=64 --set scenarios=65536"

echo.
echo ============================================================
echo LECTURE CORE DEMO COMPLETE
echo Results written to:
echo   %DEMO_JSONL%
echo ============================================================
exit /b 0

:run
echo.
echo ------------------------------------------------------------
echo DEMO: %~1
echo ------------------------------------------------------------
REM The second argument is a quoted command fragment.
REM Do not use SHIFT + %* here: in cmd.exe, %* expands to the original
REM subroutine argument list, so the demo label would be passed to
REM gpu_algobench.exe and trigger the usage/help path.
set "DEMO_ARGS=%~2"
echo COMMAND: "%GPU_ALGOBENCH%" %DEMO_ARGS% --repeat %DEMO_REPEAT% --warmup %DEMO_WARMUP% --output "%DEMO_JSONL%"
"%GPU_ALGOBENCH%" %DEMO_ARGS% --repeat %DEMO_REPEAT% --warmup %DEMO_WARMUP% --output "%DEMO_JSONL%"
if errorlevel 1 exit /b 1
exit /b 0
