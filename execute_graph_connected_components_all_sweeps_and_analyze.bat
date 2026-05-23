@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph connected-components all-in-one sweep + analysis + plotting flow
REM
REM This is the canonical connected-components experiment runner.
REM It runs several graph families across increasing sizes, writes one JSONL,
REM analyzes the resulting file, and generates plots showing CPU / naive-GPU / non-naive-GPU timing,
REM crossover points, and GPU convergence behavior.
REM
REM Change values below once.
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"

set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "PLOT_SHOW=0"

set "CC_REPEAT=3"
set "CC_WARMUP=1"
set "OVERWRITE_RESULTS=1"

REM 0 keeps the run laptop-friendly while still testing all graph families.
REM 1 adds the largest stress points.
set "INCLUDE_HEAVY_CASES=0"

set "RESULTS_DIR=results"
set "CC_RESULTS_FILE=graph_connected_components_shape_scale_sweep.jsonl"
set "CC_ANALYSIS_DIR=graph_connected_components_shape_scale_analysis"
set "CC_PLOTS_DIR=graph_connected_components_shape_scale_plots"

REM Random-cluster defaults.
set "RANDOM_OUT_DEGREE=4"
set "RANDOM_SEED=20260523"

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
set "CC_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%CC_RESULTS_FILE%"
set "CC_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CC_ANALYSIS_DIR%"
set "CC_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CC_PLOTS_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_graph_connected_components_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_graph_connected_components_scaling.py"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found:
    echo   %GPU_ALGOBENCH%
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" (
    mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
)

if "%OVERWRITE_RESULTS%"=="1" (
    if exist "%CC_JSONL%" del "%CC_JSONL%"
)

set "GPU_FLAG="
set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

REM ============================================================
REM Sweep execution
REM
REM Each benchmark call now emits cpu, gpu, and gpu-non-naive rows
REM when CUDA is enabled. The analyzer/plotter compare all three.
REM ============================================================

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Connected-components shape x scale sweep
    echo ============================================================
    echo Output JSONL:
    echo   %CC_JSONL%
    echo.

    REM ------------------------------------------------------------
    REM Chains: intentionally bad GPU-oriented anatomy.
    REM High component diameter means label propagation needs many rounds.
    REM ------------------------------------------------------------
    echo --- chains ---
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=chains --set components=8 --set component_size=64 --set sweep_label=chains_8x64 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=chains --set components=16 --set component_size=128 --set sweep_label=chains_16x128 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=chains --set components=32 --set component_size=256 --set sweep_label=chains_32x256 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=chains --set components=64 --set component_size=512 --set sweep_label=chains_64x512 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=chains --set components=128 --set component_size=1024 --set sweep_label=chains_128x1024 --output "%CC_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Grid components: structured locality and moderate diameter.
    REM ------------------------------------------------------------
    echo --- grid_components ---
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=grid_components --set components=4 --set grid_width=16 --set grid_height=16 --set sweep_label=grid_4x16x16 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=grid_components --set components=8 --set grid_width=32 --set grid_height=32 --set sweep_label=grid_8x32x32 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=grid_components --set components=16 --set grid_width=64 --set grid_height=64 --set sweep_label=grid_16x64x64 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=grid_components --set components=24 --set grid_width=96 --set grid_height=96 --set sweep_label=grid_24x96x96 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=grid_components --set components=32 --set grid_width=128 --set grid_height=128 --set sweep_label=grid_32x128x128 --output "%CC_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Random clusters: GPU-oriented family. Random shortcuts reduce
    REM effective diameter and usually reduce label-propagation iterations.
    REM ------------------------------------------------------------
    echo --- random_clusters ---
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=random_clusters --set components=8 --set component_size=128 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_8x128_d%RANDOM_OUT_DEGREE% --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=random_clusters --set components=16 --set component_size=256 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_16x256_d%RANDOM_OUT_DEGREE% --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=random_clusters --set components=32 --set component_size=512 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_32x512_d%RANDOM_OUT_DEGREE% --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=random_clusters --set components=64 --set component_size=1024 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_64x1024_d%RANDOM_OUT_DEGREE% --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=random_clusters --set components=128 --set component_size=2048 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_128x2048_d%RANDOM_OUT_DEGREE% --output "%CC_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Mixed components: deterministic mixture of chains, stars, and rings.
    REM This intentionally mixes GPU-friendly and GPU-unfriendly anatomy.
    REM ------------------------------------------------------------
    echo --- mixed ---
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=mixed --set components=8 --set component_size=128 --set sweep_label=mixed_8x128 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=mixed --set components=16 --set component_size=256 --set sweep_label=mixed_16x256 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=mixed --set components=32 --set component_size=512 --set sweep_label=mixed_32x512 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=mixed --set components=64 --set component_size=1024 --set sweep_label=mixed_64x1024 --output "%CC_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_connected_components --preset tiny --repeat %CC_REPEAT% --warmup %CC_WARMUP% --set graph=mixed --set components=128 --set component_size=2048 --set sweep_label=mixed_128x2048 --output "%CC_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

REM ============================================================
REM Analysis and plotting
REM ============================================================

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze connected-components JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%CC_JSONL%" --output-dir "%CC_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot connected-components scaling results
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --jsonl "%CC_JSONL%" --output-dir "%CC_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo CONNECTED-COMPONENTS SWEEP COMPLETE
echo JSONL:
echo   %CC_JSONL%
echo Analysis:
echo   %CC_ANALYSIS_PATH%
echo Plots:
echo   %CC_PLOTS_PATH%
echo ============================================================

endlocal
