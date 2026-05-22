@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph BFS all-in-one sweep + analysis + plotting flow
REM
REM This is the only graph-BFS batch file intended for users.
REM Older split graph-BFS batch wrappers were removed so all sweep logic
REM lives in one place.
REM
REM What this script does:
REM   1. Runs a layered-only BFS scale sweep.
REM   2. Runs a shape x scale BFS sweep over chain/grid/layered/random.
REM   3. Analyzes both JSONL files.
REM   4. Generates both plot sets.
REM
REM Change values below once.
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"

set "RUN_LAYERED_SWEEP=1"
set "RUN_SHAPE_SCALE_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "PLOT_SHOW=0"

set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RUN_GPU=1"
set "OVERWRITE_RESULTS=1"

set "RESULTS_DIR=results"
set "LAYERED_RESULTS_FILE=graph_bfs_layered_scale_sweep.jsonl"
set "SHAPE_RESULTS_FILE=graph_bfs_shape_scale_sweep.jsonl"

REM Layered-only final stress point.
REM Set to 0 for a shorter laptop-friendly run.
set "INCLUDE_VERY_LARGE=1"

REM Shape x scale optional stress points.
REM 0 still runs chain/grid/layered/random across several sizes.
REM 1 additionally runs the largest stress points, such as random_1048576.
set "INCLUDE_HEAVY_CASES=0"

REM Analyzer behavior.
REM 0 accepts fallback estimates for older JSONL files.
REM 1 requires exact frontier metrics from Phase 3.2.4+ binaries.
set "REQUIRE_EXACT_FRONTIER=0"

REM Graph-shape defaults.
set "LAYERED_EARLY_FANOUT=4"
set "LAYERED_MAIN_FANOUT=8"
set "RANDOM_OUT_DEGREE=8"
set "RANDOM_SEED=1337"

REM ============================================================
REM Path detection
REM ============================================================

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    REM Script was copied into the build directory by CMake.
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    REM Script is being run from the repository root, the canonical mode.
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "LAYERED_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%LAYERED_RESULTS_FILE%"
set "SHAPE_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%SHAPE_RESULTS_FILE%"
set "LAYERED_ANALYSIS_DIR=%PROJECT_DIR%\%RESULTS_DIR%\graph_bfs_layered_scale_analysis"
set "SHAPE_ANALYSIS_DIR=%PROJECT_DIR%\%RESULTS_DIR%\graph_bfs_shape_scale_analysis"
set "LAYERED_PLOT_DIR=%PROJECT_DIR%\%RESULTS_DIR%\graph_bfs_layered_scale_plots"
set "SHAPE_PLOT_DIR=%PROJECT_DIR%\%RESULTS_DIR%\graph_bfs_shape_scale_plots"

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
    if exist "%LAYERED_JSONL%" del "%LAYERED_JSONL%"
    if exist "%SHAPE_JSONL%" del "%SHAPE_JSONL%"
)

set "GPU_FLAG="
if "%RUN_GPU%"=="0" set "GPU_FLAG=--no-gpu"

set "SHOW_FLAG="
if "%PLOT_SHOW%"=="1" set "SHOW_FLAG=--show"

set "ANALYZER_EXACT_FLAG="
if "%REQUIRE_EXACT_FRONTIER%"=="1" set "ANALYZER_EXACT_FLAG=--require-exact-frontier"

REM ============================================================
REM Summary
REM ============================================================

echo ============================================================
echo GPU Classical Algorithms Lab - Graph BFS All Sweeps
echo ============================================================
echo Project dir:               %PROJECT_DIR%
echo Executable:                %GPU_ALGOBENCH%
echo Repeat:                    %BFS_REPEAT%
echo Warmup:                    %BFS_WARMUP%
echo RUN_GPU:                   %RUN_GPU%
echo RUN_LAYERED_SWEEP:         %RUN_LAYERED_SWEEP%
echo RUN_SHAPE_SCALE_SWEEP:     %RUN_SHAPE_SCALE_SWEEP%
echo INCLUDE_VERY_LARGE:        %INCLUDE_VERY_LARGE%
echo INCLUDE_HEAVY_CASES:       %INCLUDE_HEAVY_CASES%
echo RUN_ANALYSIS:              %RUN_ANALYSIS%
echo RUN_PLOTS:                 %RUN_PLOTS%
echo.
echo Layered JSONL:
echo   %LAYERED_JSONL%
echo Shape x scale JSONL:
echo   %SHAPE_JSONL%
echo ============================================================
echo.

if "%RUN_LAYERED_SWEEP%"=="1" (
    echo ============================================================
    echo STEP 1A: Layered-only BFS scale sweep
    echo ============================================================

    call :run_layered layered_6x16        6      16     %LAYERED_EARLY_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_8x64        8      64     %LAYERED_EARLY_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_16x256      16     256    %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_32x512      32     512    %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_48x1024     48     1024   %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_64x2048     64     2048   %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_64x4096     64     4096   %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    call :run_layered layered_96x8192     96     8192   %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
    if errorlevel 1 exit /b 1

    if "%INCLUDE_VERY_LARGE%"=="1" (
        call :run_layered layered_128x16384 128  16384  %LAYERED_MAIN_FANOUT% "%LAYERED_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_SHAPE_SCALE_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1B: Shape x scale BFS sweep chain/grid/layered/random
    echo ============================================================

    REM Chain: intentionally bad GPU case. Frontier width is about 1.
    call :run_chain chain_256     256     "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_chain chain_1024    1024    "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_chain chain_4096    4096    "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_chain chain_8192    8192    "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_chain chain_16384   16384   "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        call :run_chain chain_65536 65536 "%SHAPE_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM Grid: structured local wavefront. More parallel than chain, but many BFS levels.
    call :run_grid grid_16x16     16      16      "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_grid grid_64x64     64      64      "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_grid grid_128x128   128     128     "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_grid grid_512x512   512     512     "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        call :run_grid grid_1024x1024 1024 1024 "%SHAPE_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM Layered: deliberately wide frontiers, usually the best regular case.
    call :run_layered layered_6x16       6      16      %LAYERED_EARLY_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_layered layered_8x64       8      64      %LAYERED_EARLY_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_layered layered_16x256     16     256     %LAYERED_MAIN_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_layered layered_32x512     32     512     %LAYERED_MAIN_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_layered layered_64x4096    64     4096    %LAYERED_MAIN_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_layered layered_96x8192    96     8192    %LAYERED_MAIN_FANOUT% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        call :run_layered layered_128x16384 128 16384 %LAYERED_MAIN_FANOUT% "%SHAPE_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM Random sparse: low depth and large frontier bursts. Often where this simple GPU BFS wins.
    call :run_random random_256       256       %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_random random_4096      4096      %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_random random_16384     16384     %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_random random_65536     65536     %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    call :run_random random_262144    262144    %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        call :run_random random_1048576 1048576 %RANDOM_OUT_DEGREE% "%SHAPE_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_ANALYSIS%"=="1" (
    if exist "%LAYERED_JSONL%" (
        echo.
        echo ============================================================
        echo STEP 2A: Analyze layered-only JSONL
        echo ============================================================
        %PYTHON% "%PROJECT_DIR%\scripts\analyze_graph_bfs_jsonl.py" --jsonl "%LAYERED_JSONL%" --output-dir "%LAYERED_ANALYSIS_DIR%" %ANALYZER_EXACT_FLAG%
        if errorlevel 1 exit /b 1
    )

    if exist "%SHAPE_JSONL%" (
        echo.
        echo ============================================================
        echo STEP 2B: Analyze shape x scale JSONL
        echo ============================================================
        %PYTHON% "%PROJECT_DIR%\scripts\analyze_graph_bfs_jsonl.py" --jsonl "%SHAPE_JSONL%" --output-dir "%SHAPE_ANALYSIS_DIR%" %ANALYZER_EXACT_FLAG%
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_PLOTS%"=="1" (
    if exist "%LAYERED_JSONL%" (
        echo.
        echo ============================================================
        echo STEP 3A: Plot layered-only scaling
        echo ============================================================
        %PYTHON% "%PROJECT_DIR%\scripts\plot_graph_bfs_scaling.py" --jsonl "%LAYERED_JSONL%" --output-dir "%LAYERED_PLOT_DIR%" %SHOW_FLAG%
        if errorlevel 1 exit /b 1
    )

    if exist "%SHAPE_JSONL%" (
        echo.
        echo ============================================================
        echo STEP 3B: Plot shape x scale scaling
        echo ============================================================
        %PYTHON% "%PROJECT_DIR%\scripts\plot_graph_bfs_shape_scaling.py" --jsonl "%SHAPE_JSONL%" --output-dir "%SHAPE_PLOT_DIR%" %SHOW_FLAG%
        if errorlevel 1 exit /b 1
    )
)

echo.
echo ============================================================
echo GRAPH BFS ALL-IN-ONE FLOW FINISHED

echo.
echo Layered-only results:
echo   JSONL:     %LAYERED_JSONL%
echo   Analysis:  %LAYERED_ANALYSIS_DIR%
echo   Plots:     %LAYERED_PLOT_DIR%
echo.
echo Shape x scale results:
echo   JSONL:     %SHAPE_JSONL%
echo   Analysis:  %SHAPE_ANALYSIS_DIR%
echo   Plots:     %SHAPE_PLOT_DIR%
echo.
echo The shape x scale JSONL includes:
echo   chain, grid, layered, random
echo ============================================================
exit /b 0

REM ============================================================
REM Subroutines
REM ============================================================

:run_chain
set "SWEEP_LABEL=%~1"
set "CHAIN_NODES=%~2"
set "OUTPUT_JSONL=%~3"

echo ------------------------------------------------------------
echo GRAPH BFS SHAPE/SCALE: %SWEEP_LABEL%
echo   graph=chain nodes=%CHAIN_NODES%
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set sweep_label=%SWEEP_LABEL% --set graph=chain --set chain_nodes=%CHAIN_NODES% %GPU_FLAG% --output "%OUTPUT_JSONL%"
if errorlevel 1 (
    echo FAILED: %SWEEP_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_grid
set "SWEEP_LABEL=%~1"
set "GRID_WIDTH=%~2"
set "GRID_HEIGHT=%~3"
set "OUTPUT_JSONL=%~4"

echo ------------------------------------------------------------
echo GRAPH BFS SHAPE/SCALE: %SWEEP_LABEL%
echo   graph=grid width=%GRID_WIDTH% height=%GRID_HEIGHT%
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set sweep_label=%SWEEP_LABEL% --set graph=grid --set grid_width=%GRID_WIDTH% --set grid_height=%GRID_HEIGHT% %GPU_FLAG% --output "%OUTPUT_JSONL%"
if errorlevel 1 (
    echo FAILED: %SWEEP_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_layered
set "SWEEP_LABEL=%~1"
set "LAYERS=%~2"
set "NODES_PER_LAYER=%~3"
set "FANOUT=%~4"
set "OUTPUT_JSONL=%~5"

echo ------------------------------------------------------------
echo GRAPH BFS SHAPE/SCALE: %SWEEP_LABEL%
echo   graph=layered layers=%LAYERS% nodes_per_layer=%NODES_PER_LAYER% fanout=%FANOUT%
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set sweep_label=%SWEEP_LABEL% --set graph=layered --set layers=%LAYERS% --set nodes_per_layer=%NODES_PER_LAYER% --set fanout=%FANOUT% %GPU_FLAG% --output "%OUTPUT_JSONL%"
if errorlevel 1 (
    echo FAILED: %SWEEP_LABEL%
    exit /b 1
)
echo.
exit /b 0

:run_random
set "SWEEP_LABEL=%~1"
set "RANDOM_NODES=%~2"
set "RANDOM_DEGREE=%~3"
set "OUTPUT_JSONL=%~4"

echo ------------------------------------------------------------
echo GRAPH BFS SHAPE/SCALE: %SWEEP_LABEL%
echo   graph=random nodes=%RANDOM_NODES% out_degree=%RANDOM_DEGREE% seed=%RANDOM_SEED%
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset tiny --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set sweep_label=%SWEEP_LABEL% --set graph=random --set random_nodes=%RANDOM_NODES% --set random_out_degree=%RANDOM_DEGREE% --set seed=%RANDOM_SEED% %GPU_FLAG% --output "%OUTPUT_JSONL%"
if errorlevel 1 (
    echo FAILED: %SWEEP_LABEL%
    exit /b 1
)
echo.
exit /b 0
