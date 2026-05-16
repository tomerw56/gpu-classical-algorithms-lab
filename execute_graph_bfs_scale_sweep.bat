@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph BFS layered scale-sweep runner
REM
REM This script asks a focused question:
REM   As a layered graph grows from tiny to very large, does the
REM   educational GPU frontier BFS catch up with or beat CPU queue BFS?
REM
REM Change these values at the top of the file once.
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RESULTS_DIR=results"
set "RESULTS_FILE=graph_bfs_layered_scale_sweep.jsonl"

REM Set to 1 to include GPU rows when the binary was built with CUDA.
REM Set to 0 to force CPU-only rows.
set "RUN_GPU=1"

REM Set to 1 to delete previous sweep results before running.
REM Set to 0 to append and let the Python plotter aggregate repeated sweeps.
set "OVERWRITE_RESULTS=1"

REM Set to 1 to include the final very-large case.
REM It is useful when searching for a CPU/GPU crossover, but it can require
REM more graph-build time and memory than the earlier points.
set "INCLUDE_VERY_LARGE=1"

REM Fanout defaults used by the scale points below.
set "EARLY_FANOUT=4"
set "MAIN_FANOUT=8"

REM ============================================================
REM Path detection
REM ============================================================

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "RESULTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%RESULTS_FILE%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: Could not find gpu_algobench.exe.
    echo Expected path:
    echo   %GPU_ALGOBENCH%
    echo.
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" (
    mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
)

if "%OVERWRITE_RESULTS%"=="1" (
    if exist "%RESULTS_PATH%" del "%RESULTS_PATH%"
)

set "GPU_FLAG="
if "%RUN_GPU%"=="0" set "GPU_FLAG=--no-gpu"

REM ============================================================
REM Summary
REM ============================================================

echo ============================================================
echo GPU Classical Algorithms Lab - Graph BFS Layered Scale Sweep
echo ============================================================
echo Project dir:        %PROJECT_DIR%
echo Executable dir:     %EXE_DIR%
echo Repeat:             %BFS_REPEAT%
echo Warmup:             %BFS_WARMUP%
echo RUN_GPU:            %RUN_GPU%
echo INCLUDE_VERY_LARGE: %INCLUDE_VERY_LARGE%
echo Results:            %RESULTS_PATH%
echo ============================================================
echo.
echo NOTE: The benchmark table reports total measured time across repeats.
echo NOTE: scripts\plot_graph_bfs_scaling.py normalizes to average ms per BFS run.
echo NOTE: The sweep uses layered graphs because they are the most GPU-friendly
echo       shape for the current educational frontier BFS.
echo.

REM ============================================================
REM Scale points
REM Each call uses: label, layers, nodes_per_layer, fanout
REM Node count is layers * nodes_per_layer.
REM ============================================================

call :run_layered tiny_debug       6      16     %EARLY_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered tiny_wider       8      64     %EARLY_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered smallish        16     256     %MAIN_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered repository_small 32    512     %MAIN_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered mid             48     1024    %MAIN_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered larger          64     2048    %MAIN_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered repository_medium 64   4096    %MAIN_FANOUT%
if errorlevel 1 exit /b 1

call :run_layered repository_large 96    8192    %MAIN_FANOUT%
if errorlevel 1 exit /b 1

if "%INCLUDE_VERY_LARGE%"=="1" (
    call :run_layered very_large 128 16384 %MAIN_FANOUT%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo GRAPH BFS LAYERED SCALE SWEEP FINISHED
echo Results written to:
echo   %RESULTS_PATH%
echo.
echo Next, plot the sweep with:
echo   python scripts\plot_graph_bfs_scaling.py --jsonl "%RESULTS_PATH%" --output-dir results\graph_bfs_layered_scale_plots --show
echo ============================================================
exit /b 0

:run_layered
set "SCALE_LABEL=%~1"
set "LAYERS=%~2"
set "NODES_PER_LAYER=%~3"
set "FANOUT=%~4"

echo ------------------------------------------------------------
echo LAYERED BFS SCALE: %SCALE_LABEL%
echo   layers=%LAYERS% nodes_per_layer=%NODES_PER_LAYER% fanout=%FANOUT%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=layered --set layers=%LAYERS% --set nodes_per_layer=%NODES_PER_LAYER% --set fanout=%FANOUT% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=layered --set layers=%LAYERS% --set nodes_per_layer=%NODES_PER_LAYER% --set fanout=%FANOUT% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: layered scale point %SCALE_LABEL%
    exit /b 1
)
echo.
exit /b 0
