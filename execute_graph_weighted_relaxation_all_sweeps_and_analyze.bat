@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph weighted-relaxation all-in-one sweep + analysis + plotting flow
REM
REM This is the canonical weighted shortest-path experiment runner.
REM It runs CPU Dijkstra and GPU iterative edge-relaxation over several graph
REM families and sizes, writes one JSONL, analyzes it, and generates plots.
REM
REM Change values below once.
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"

set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "RUN_DELTA_TUNING=1"
set "PLOT_SHOW=0"

set "WR_REPEAT=3"
set "WR_WARMUP=1"
set "OVERWRITE_RESULTS=1"

REM 0 keeps the run laptop-friendly while still testing all graph families.
REM 1 adds the largest stress points. Be careful: chain/grid weighted relaxation
REM can need many iterations in the naive GPU algorithm.
set "INCLUDE_HEAVY_CASES=0"

REM Adds an explicit very-very-large random graph point to make the random-graph
REM conclusion clear. This is separate from INCLUDE_HEAVY_CASES because random
REM is the family where GPU global scan is expected to win decisively.
REM Keep the repeat lower for this stress point so the full flow remains usable.
set "INCLUDE_VERY_VERY_LARGE_RANDOM=1"
set "VERY_VERY_LARGE_RANDOM_NODES=1048576"
set "VERY_VERY_LARGE_RANDOM_REPEAT=1"

set "RESULTS_DIR=results"
set "WR_RESULTS_FILE=graph_weighted_relaxation_shape_scale_sweep.jsonl"
set "WR_ANALYSIS_DIR=graph_weighted_relaxation_shape_scale_analysis"
set "WR_PLOTS_DIR=graph_weighted_relaxation_shape_scale_plots"
set "WR_RECOMMENDATION_DIR=graph_weighted_relaxation_backend_recommendations"
set "WR_DELTA_TUNING_FILE=graph_weighted_relaxation_delta_tuning.jsonl"
set "WR_DELTA_TUNING_PLOTS_DIR=graph_weighted_relaxation_delta_tuning_plots"

REM Random graph defaults.
set "RANDOM_OUT_DEGREE=8"
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
set "WR_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%WR_RESULTS_FILE%"
set "WR_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%WR_ANALYSIS_DIR%"
set "WR_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%WR_PLOTS_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_graph_weighted_relaxation_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_graph_weighted_relaxation_scaling.py"
set "RECOMMEND_SCRIPT=%PROJECT_DIR%\scripts\recommend_graph_weighted_relaxation_backend.py"
set "DELTA_TUNING_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_graph_weighted_delta_tuning.py"
set "WR_DELTA_TUNING_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%WR_DELTA_TUNING_FILE%"
set "WR_DELTA_TUNING_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%WR_DELTA_TUNING_PLOTS_DIR%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found:
    echo   %GPU_ALGOBENCH%
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%ANALYZE_SCRIPT%" (
    echo ERROR: analysis script was not found:
    echo   %ANALYZE_SCRIPT%
    exit /b 1
)

if not exist "%PLOT_SCRIPT%" (
    echo ERROR: plot script was not found:
    echo   %PLOT_SCRIPT%
    exit /b 1
)

if not exist "%RECOMMEND_SCRIPT%" (
    echo ERROR: recommendation script was not found:
    echo   %RECOMMEND_SCRIPT%
    exit /b 1
)

if not exist "%DELTA_TUNING_PLOT_SCRIPT%" (
    echo ERROR: delta tuning plot script was not found:
    echo   %DELTA_TUNING_PLOT_SCRIPT%
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" (
    mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
)

if "%OVERWRITE_RESULTS%"=="1" (
    if exist "%WR_JSONL%" del "%WR_JSONL%"
    if exist "%WR_DELTA_TUNING_JSONL%" del "%WR_DELTA_TUNING_JSONL%"
)

set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

REM ============================================================
REM Sweep execution
REM ============================================================

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Weighted-relaxation shape x scale sweep
    echo ============================================================
    echo Output JSONL:
    echo   %WR_JSONL%
    echo.

    REM ------------------------------------------------------------
    REM Chain: path-like anatomy. Good CPU Dijkstra case; very hard for
    REM iterative edge-relaxation because weighted distances propagate through
    REM many levels.
    REM ------------------------------------------------------------
    echo --- chain ---
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=chain --set chain_nodes=256 --set sweep_label=chain_256 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=chain --set chain_nodes=1024 --set sweep_label=chain_1024 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=chain --set chain_nodes=4096 --set sweep_label=chain_4096 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=chain --set chain_nodes=16384 --set sweep_label=chain_16384 --output "%WR_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Grid: local wavefront-like structure. Dijkstra is strong; GPU
    REM relaxation gets more work, but may still pay many global passes.
    REM ------------------------------------------------------------
    echo --- grid ---
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=grid --set grid_width=16 --set grid_height=16 --set sweep_label=grid_16x16 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=grid --set grid_width=64 --set grid_height=64 --set sweep_label=grid_64x64 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=grid --set grid_width=128 --set grid_height=128 --set sweep_label=grid_128x128 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=grid --set grid_width=256 --set grid_height=256 --set sweep_label=grid_256x256 --output "%WR_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Layered: fewer relaxation rounds than chain/grid, with more edge work
    REM per level. This is usually a better GPU-oriented graph family.
    REM ------------------------------------------------------------
    echo --- layered ---
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=8 --set nodes_per_layer=64 --set fanout=4 --set sweep_label=layered_8x64_f4 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=16 --set nodes_per_layer=256 --set fanout=6 --set sweep_label=layered_16x256_f6 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=32 --set nodes_per_layer=512 --set fanout=6 --set sweep_label=layered_32x512_f6 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=64 --set nodes_per_layer=2048 --set fanout=8 --set sweep_label=layered_64x2048_f8 --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=96 --set nodes_per_layer=8192 --set fanout=8 --set sweep_label=layered_96x8192_f8 --output "%WR_JSONL%"
        if errorlevel 1 exit /b 1
    )

    REM ------------------------------------------------------------
    REM Random: low effective diameter and high edge parallelism. Often the
    REM best case for global edge-relaxation, subject to atomic contention.
    REM ------------------------------------------------------------
    echo --- random ---
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=1024 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_1024_d%RANDOM_OUT_DEGREE% --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=16384 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_16384_d%RANDOM_OUT_DEGREE% --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=65536 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_65536_d%RANDOM_OUT_DEGREE% --output "%WR_JSONL%"
    if errorlevel 1 exit /b 1
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=262144 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_262144_d%RANDOM_OUT_DEGREE% --output "%WR_JSONL%"
        if errorlevel 1 exit /b 1
    )
    if "%INCLUDE_VERY_VERY_LARGE_RANDOM%"=="1" (
        echo --- random very very large ---
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %VERY_VERY_LARGE_RANDOM_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=%VERY_VERY_LARGE_RANDOM_NODES% --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set sweep_label=random_%VERY_VERY_LARGE_RANDOM_NODES%_d%RANDOM_OUT_DEGREE% --output "%WR_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

REM ============================================================
REM Analysis
REM ============================================================

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze weighted-relaxation JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%WR_JSONL%" --output-dir "%WR_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

REM ============================================================
REM Plotting
REM ============================================================

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot weighted-relaxation sweep
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --jsonl "%WR_JSONL%" --output-dir "%WR_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

REM ============================================================
REM Backend recommendation
REM ============================================================

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 4: Recommend fastest measured backend
    echo ============================================================
    "%PYTHON%" "%RECOMMEND_SCRIPT%" --summary-csv "%WR_ANALYSIS_PATH%\graph_weighted_relaxation_analysis_summary.csv" --output-dir "%PROJECT_DIR%\%RESULTS_DIR%\%WR_RECOMMENDATION_DIR%"
    if errorlevel 1 exit /b 1
)


REM ============================================================
REM Delta parameter tuning
REM ============================================================

if "%RUN_DELTA_TUNING%"=="1" (
    echo.
    echo ============================================================
    echo STEP 5: Delta parameter tuning
    echo ============================================================
    echo Output JSONL:
    echo   %WR_DELTA_TUNING_JSONL%
    echo.
    echo This step checks whether the poor delta-stepping result is a bad
    echo delta value or a more fundamental overhead issue.
    echo.

    echo --- delta tuning: layered_64x2048_f8 ---
    for %%D in (2 4 8 16 32 64 128) do (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=layered --set layers=64 --set nodes_per_layer=2048 --set fanout=8 --set delta=%%D --set sweep_label=layered_64x2048_f8_delta%%D --output "%WR_DELTA_TUNING_JSONL%"
        if errorlevel 1 exit /b 1
    )

    echo --- delta tuning: random_16384_d8 ---
    for %%D in (2 4 8 16 32 64 128) do (
        "%GPU_ALGOBENCH%" --benchmark graph_weighted_relaxation --preset tiny --repeat %WR_REPEAT% --warmup %WR_WARMUP% --set graph=random --set random_nodes=16384 --set random_out_degree=%RANDOM_OUT_DEGREE% --set seed=%RANDOM_SEED% --set delta=%%D --set sweep_label=random_16384_d%RANDOM_OUT_DEGREE%_delta%%D --output "%WR_DELTA_TUNING_JSONL%"
        if errorlevel 1 exit /b 1
    )

    echo.
    echo ============================================================
    echo STEP 6: Plot delta tuning
    echo ============================================================
    "%PYTHON%" "%DELTA_TUNING_PLOT_SCRIPT%" --jsonl "%WR_DELTA_TUNING_JSONL%" --output-dir "%WR_DELTA_TUNING_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo WEIGHTED RELAXATION SWEEP COMPLETE
echo JSONL:
echo   %WR_JSONL%
echo Analysis:
echo   %WR_ANALYSIS_PATH%
echo Plots:
echo   %WR_PLOTS_PATH%
echo Recommendations:
echo   %PROJECT_DIR%\%RESULTS_DIR%\%WR_RECOMMENDATION_DIR%
echo Delta tuning JSONL:
echo   %WR_DELTA_TUNING_JSONL%
echo Delta tuning plots:
echo   %WR_DELTA_TUNING_PLOTS_PATH%
echo ============================================================

endlocal
