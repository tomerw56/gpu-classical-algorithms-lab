@echo off
setlocal EnableExtensions

REM ============================================================
REM Combination finder all-in-one sweep + analysis + plotting flow
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"

set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "PLOT_SHOW=0"

set "COMB_REPEAT=3"
set "COMB_WARMUP=1"
set "OVERWRITE_RESULTS=1"

REM 0 keeps the default run laptop-friendly. 1 adds larger combinatorial cases.
set "INCLUDE_HEAVY_CASES=0"

set "RESULTS_DIR=results"
set "COMB_RESULTS_FILE=combination_finder_scale_sweep.jsonl"
set "COMB_ANALYSIS_DIR=combination_finder_analysis"
set "COMB_PLOTS_DIR=combination_finder_plots"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "COMB_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%COMB_RESULTS_FILE%"
set "COMB_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%COMB_ANALYSIS_DIR%"
set "COMB_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%COMB_PLOTS_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_combination_finder_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_combination_finder_scaling.py"

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
    if exist "%COMB_JSONL%" del "%COMB_JSONL%"
)

set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Combination-finder shape x scale sweep
    echo ============================================================
    echo Output JSONL:
    echo   %COMB_JSONL%
    echo.

    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=64 --set k=3 --set sweep_label=comb_64_choose_3 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=128 --set k=3 --set sweep_label=comb_128_choose_3 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=192 --set k=3 --set sweep_label=comb_192_choose_3 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=256 --set k=3 --set sweep_label=comb_256_choose_3 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=384 --set k=3 --set sweep_label=comb_384_choose_3 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat %COMB_REPEAT% --warmup %COMB_WARMUP% --set items=128 --set k=4 --set sweep_label=comb_128_choose_4 --output "%COMB_JSONL%"
    if errorlevel 1 exit /b 1

    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat 1 --warmup %COMB_WARMUP% --set items=512 --set k=3 --set sweep_label=comb_512_choose_3 --output "%COMB_JSONL%"
        if errorlevel 1 exit /b 1
        "%GPU_ALGOBENCH%" --benchmark combination_finder --preset tiny --repeat 1 --warmup %COMB_WARMUP% --set items=192 --set k=4 --set sweep_label=comb_192_choose_4 --output "%COMB_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze combination-finder JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%COMB_JSONL%" --output-dir "%COMB_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot combination-finder results
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --summary "%COMB_ANALYSIS_PATH%\combination_finder_analysis_summary.csv" --output-dir "%COMB_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo COMBINATION FINDER SWEEP COMPLETE
echo Results:
echo   %COMB_JSONL%
echo Analysis:
echo   %COMB_ANALYSIS_PATH%
echo Plots:
echo   %COMB_PLOTS_PATH%
echo ============================================================

endlocal
