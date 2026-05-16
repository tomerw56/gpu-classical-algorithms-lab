@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph BFS graph-shape x scale sweep runner
REM
REM This script asks a broader question than the layered-only sweep:
REM   How do graph size AND graph anatomy affect CPU vs GPU BFS?
REM
REM It runs the same graph_bfs benchmark over several graph families:
REM   chain, grid, layered, random
REM
REM Change these values at the top of the file once.
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RESULTS_DIR=results"
set "RESULTS_FILE=graph_bfs_shape_scale_sweep.jsonl"

REM Set to 1 to include GPU rows when the binary was built with CUDA.
REM Set to 0 to force CPU-only rows.
set "RUN_GPU=1"

REM Set to 1 to delete previous sweep results before running.
REM Set to 0 to append and let the Python plotter aggregate repeated sweeps.
set "OVERWRITE_RESULTS=1"

REM Optional heavier cases. They can be useful when searching for a GPU
REM crossover, but they are intentionally disabled by default so this script
REM remains practical on laptops and can be rerun often.
set "INCLUDE_HEAVY_CASES=1"

REM Parameters shared by the scale points below.
set "LAYERED_FANOUT=8"
set "RANDOM_OUT_DEGREE=8"
set "RANDOM_SEED=1337"

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
echo GPU Classical Algorithms Lab - Graph BFS Shape x Scale Sweep
echo ============================================================
echo Project dir:         %PROJECT_DIR%
echo Executable dir:      %EXE_DIR%
echo Repeat:              %BFS_REPEAT%
echo Warmup:              %BFS_WARMUP%
echo RUN_GPU:             %RUN_GPU%
echo INCLUDE_HEAVY_CASES:  %INCLUDE_HEAVY_CASES%
echo Results:             %RESULTS_PATH%
echo ============================================================
echo.
echo NOTE: This sweep intentionally compares graph anatomy as well as size.
echo NOTE: Chain graphs have tiny frontiers; layered and low-diameter random
echo       graphs may expose more useful GPU parallelism.
echo NOTE: The benchmark table reports total measured time across repeats.
echo NOTE: scripts\plot_graph_bfs_shape_scaling.py normalizes to average ms/run.
echo NOTE: The runner uses --preset tiny only as a neutral base before explicit --set dimensions.
echo NOTE: Sweep rows now display the scale label, for example layered_64x4096, in the preset column.
echo.

REM ============================================================
REM CHAIN: pathologically narrow frontiers.
REM Keep default sizes conservative because BFS depth equals node count.
REM ============================================================

call :run_chain chain_256      256
if errorlevel 1 exit /b 1
call :run_chain chain_1024     1024
if errorlevel 1 exit /b 1
call :run_chain chain_4096     4096
if errorlevel 1 exit /b 1
call :run_chain chain_8192     8192
if errorlevel 1 exit /b 1

if "%INCLUDE_HEAVY_CASES%"=="1" (
    call :run_chain chain_16384 16384
    if errorlevel 1 exit /b 1
)

REM ============================================================
REM GRID: local wavefronts with graph diameter tied to width + height.
REM ============================================================

call :run_grid grid_16x16       16   16
if errorlevel 1 exit /b 1
call :run_grid grid_64x64       64   64
if errorlevel 1 exit /b 1
call :run_grid grid_128x128     128  128
if errorlevel 1 exit /b 1
call :run_grid grid_256x256     256  256
if errorlevel 1 exit /b 1

if "%INCLUDE_HEAVY_CASES%"=="1" (
    call :run_grid grid_512x512 512 512
    if errorlevel 1 exit /b 1
)

REM ============================================================
REM LAYERED: intentionally wide frontiers; friendliest current GPU shape.
REM ============================================================

call :run_layered layered_6x16        6   16    4
if errorlevel 1 exit /b 1
call :run_layered layered_8x64        8   64    4
if errorlevel 1 exit /b 1
call :run_layered layered_16x256      16  256   %LAYERED_FANOUT%
if errorlevel 1 exit /b 1
call :run_layered layered_32x512      32  512   %LAYERED_FANOUT%
if errorlevel 1 exit /b 1
call :run_layered layered_64x4096     64  4096  %LAYERED_FANOUT%
if errorlevel 1 exit /b 1

if "%INCLUDE_HEAVY_CASES%"=="1" (
    call :run_layered layered_96x8192 96 8192 %LAYERED_FANOUT%
    if errorlevel 1 exit /b 1
)

REM ============================================================
REM RANDOM: controlled sparse random topology. These graphs often have lower
REM diameter than chain/grid cases and can generate large frontier bursts.
REM ============================================================

call :run_random random_256       256      %RANDOM_OUT_DEGREE%  %RANDOM_SEED%
if errorlevel 1 exit /b 1
call :run_random random_4096      4096     %RANDOM_OUT_DEGREE%  %RANDOM_SEED%
if errorlevel 1 exit /b 1
call :run_random random_16384     16384    %RANDOM_OUT_DEGREE%  %RANDOM_SEED%
if errorlevel 1 exit /b 1
call :run_random random_65536     65536    %RANDOM_OUT_DEGREE%  %RANDOM_SEED%
if errorlevel 1 exit /b 1
call :run_random random_262144    262144   %RANDOM_OUT_DEGREE%  %RANDOM_SEED%
if errorlevel 1 exit /b 1

if "%INCLUDE_HEAVY_CASES%"=="1" (
    call :run_random random_1048576 1048576 %RANDOM_OUT_DEGREE% %RANDOM_SEED%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo GRAPH BFS SHAPE x SCALE SWEEP FINISHED
echo Results written to:
echo   %RESULTS_PATH%
echo.
echo Next, plot the sweep with:
echo   python scripts\plot_graph_bfs_shape_scaling.py --jsonl "%RESULTS_PATH%" --output-dir results\graph_bfs_shape_scale_plots --show
echo ============================================================
exit /b 0

:run_chain
set "SCALE_LABEL=%~1"
set "CHAIN_NODES=%~2"
echo ------------------------------------------------------------
echo CHAIN BFS SCALE: %SCALE_LABEL%
echo   chain_nodes=%CHAIN_NODES%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=chain --set chain_nodes=%CHAIN_NODES% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=chain --set chain_nodes=%CHAIN_NODES% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: chain scale point %SCALE_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_grid
set "SCALE_LABEL=%~1"
set "GRID_WIDTH=%~2"
set "GRID_HEIGHT=%~3"
echo ------------------------------------------------------------
echo GRID BFS SCALE: %SCALE_LABEL%
echo   grid_width=%GRID_WIDTH% grid_height=%GRID_HEIGHT%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=grid --set grid_width=%GRID_WIDTH% --set grid_height=%GRID_HEIGHT% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=grid --set grid_width=%GRID_WIDTH% --set grid_height=%GRID_HEIGHT% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: grid scale point %SCALE_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_layered
set "SCALE_LABEL=%~1"
set "LAYERS=%~2"
set "NODES_PER_LAYER=%~3"
set "FANOUT=%~4"
echo ------------------------------------------------------------
echo LAYERED BFS SCALE: %SCALE_LABEL%
echo   layers=%LAYERS% nodes_per_layer=%NODES_PER_LAYER% fanout=%FANOUT%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=layered --set layers=%LAYERS% --set nodes_per_layer=%NODES_PER_LAYER% --set fanout=%FANOUT% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=layered --set layers=%LAYERS% --set nodes_per_layer=%NODES_PER_LAYER% --set fanout=%FANOUT% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: layered scale point %SCALE_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_random
set "SCALE_LABEL=%~1"
set "RANDOM_NODES=%~2"
set "OUT_DEGREE=%~3"
set "SEED=%~4"
echo ------------------------------------------------------------
echo RANDOM BFS SCALE: %SCALE_LABEL%
echo   random_nodes=%RANDOM_NODES% random_out_degree=%OUT_DEGREE% seed=%SEED%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=random --set random_nodes=%RANDOM_NODES% --set random_out_degree=%OUT_DEGREE% --set seed=%SEED% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=random --set random_nodes=%RANDOM_NODES% --set random_out_degree=%OUT_DEGREE% --set seed=%SEED% --set sweep_label=%SCALE_LABEL% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: random scale point %SCALE_LABEL%
    exit /b 1
)
echo.
exit /b 0
