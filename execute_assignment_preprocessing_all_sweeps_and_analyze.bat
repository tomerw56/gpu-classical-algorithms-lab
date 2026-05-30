@echo off
setlocal EnableExtensions

REM ============================================================
REM Assignment preprocessing sweep + analysis + plotting flow
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "AP_REPEAT=3"
set "AP_WARMUP=1"
set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "RUN_PROBLEM_EXPORT=1"
set "PLOT_SHOW=0"
set "OVERWRITE_RESULTS=1"
set "INCLUDE_HEAVY_CASES=0"

set "RESULTS_DIR=results"
set "AP_RESULTS_FILE=assignment_preprocessing_scale_sweep.jsonl"
set "AP_ANALYSIS_DIR=assignment_preprocessing_analysis"
set "AP_PLOTS_DIR=assignment_preprocessing_plots"
set "AP_PROBLEM_DIR=assignment_preprocessing_problem_demo"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "EXPORTER=%EXE_DIR%\export_assignment_preprocessing.exe"
set "AP_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%AP_RESULTS_FILE%"
set "AP_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_ANALYSIS_DIR%"
set "AP_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_PLOTS_DIR%"
set "AP_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_PROBLEM_DIR%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found:
    echo   %GPU_ALGOBENCH%
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
if "%OVERWRITE_RESULTS%"=="1" if exist "%AP_JSONL%" del "%AP_JSONL%"

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Assignment preprocessing task/resource sweep
    echo ============================================================
    echo Output JSONL:
    echo   %AP_JSONL%
    echo.

    "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat %AP_REPEAT% --warmup %AP_WARMUP% --set tasks=128 --set resources=256 --set top_k=4 --set sweep_label=ap_128x256_k4 --output "%AP_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat %AP_REPEAT% --warmup %AP_WARMUP% --set tasks=512 --set resources=512 --set top_k=8 --set sweep_label=ap_512x512_k8 --output "%AP_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat %AP_REPEAT% --warmup %AP_WARMUP% --set tasks=1024 --set resources=1024 --set top_k=8 --set sweep_label=ap_1024x1024_k8 --output "%AP_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat %AP_REPEAT% --warmup %AP_WARMUP% --set tasks=2048 --set resources=2048 --set top_k=8 --set sweep_label=ap_2048x2048_k8 --output "%AP_JSONL%"
    if errorlevel 1 exit /b 1
    "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat %AP_REPEAT% --warmup %AP_WARMUP% --set tasks=4096 --set resources=2048 --set top_k=8 --set sweep_label=ap_4096x2048_k8 --output "%AP_JSONL%"
    if errorlevel 1 exit /b 1

    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat 1 --warmup %AP_WARMUP% --set tasks=4096 --set resources=4096 --set top_k=8 --set sweep_label=ap_4096x4096_k8 --output "%AP_JSONL%"
        if errorlevel 1 exit /b 1
        "%GPU_ALGOBENCH%" --benchmark assignment_preprocessing --preset tiny --repeat 1 --warmup %AP_WARMUP% --set tasks=8192 --set resources=4096 --set top_k=8 --set sweep_label=ap_8192x4096_k8 --output "%AP_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze assignment preprocessing JSONL
    echo ============================================================
    "%PYTHON%" "%PROJECT_DIR%\scripts\analyze_assignment_preprocessing_jsonl.py" --jsonl "%AP_JSONL%" --output-dir "%AP_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot assignment preprocessing results
    echo ============================================================
    set "SHOW_ARG="
    if "%PLOT_SHOW%"=="1" set "SHOW_ARG=--show"
    "%PYTHON%" "%PROJECT_DIR%\scripts\plot_assignment_preprocessing_scaling.py" --summary-csv "%AP_ANALYSIS_PATH%\assignment_preprocessing_analysis_summary.csv" --output-dir "%AP_PLOTS_PATH%" %SHOW_ARG%
    if errorlevel 1 exit /b 1
)

if "%RUN_PROBLEM_EXPORT%"=="1" (
    echo.
    echo ============================================================
    echo STEP 4: Export and plot one inspectable assignment instance
    echo ============================================================
    if not exist "%EXPORTER%" (
        echo ERROR: export_assignment_preprocessing.exe was not found:
        echo   %EXPORTER%
        exit /b 1
    )
    "%EXPORTER%" --tasks 32 --resources 24 --top-k 4 --output-dir "%AP_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
    "%PYTHON%" "%PROJECT_DIR%\scripts\plot_assignment_preprocessing_problem.py" --input-dir "%AP_PROBLEM_PATH%" %SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo ASSIGNMENT PREPROCESSING FLOW COMPLETE
echo Results:
echo   %AP_JSONL%
echo   %AP_ANALYSIS_PATH%
echo   %AP_PLOTS_PATH%
echo   %AP_PROBLEM_PATH%
echo ============================================================

endlocal
