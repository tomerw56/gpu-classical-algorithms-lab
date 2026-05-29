@echo off
setlocal EnableExtensions

REM ============================================================
REM Constraint-network all-in-one sweep + analysis + plotting flow
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "RUN_PROBLEM_EXPORT=1"
set "PLOT_SHOW=0"
set "CN_REPEAT=5"
set "CN_WARMUP=1"
set "OVERWRITE_RESULTS=1"
REM 0 keeps the sweep laptop-friendly; 1 adds 8M and 16M candidate stress points.
set "INCLUDE_HEAVY_CASES=0"
set "RESULTS_DIR=results"
set "CN_RESULTS_FILE=constraint_network_scale_sweep.jsonl"
set "CN_ANALYSIS_DIR=constraint_network_scale_analysis"
set "CN_PLOTS_DIR=constraint_network_scale_plots"
set "CN_DIAGNOSTICS_DIR=constraint_network_diagnostics"
set "CN_PROBLEM_DIR=constraint_network_problem_demo"
set "CN_PROBLEM_PLOTS_DIR=constraint_network_problem_plots"
set "CN_PROBLEM_TASKS=32"
set "CN_PROBLEM_RESOURCES=16"
set "CN_PROBLEM_CANDIDATES=512"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "EXPORT_CONSTRAINT_NETWORK=%EXE_DIR%\export_constraint_network.exe"
set "CN_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%CN_RESULTS_FILE%"
set "CN_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CN_ANALYSIS_DIR%"
set "CN_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CN_PLOTS_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_constraint_network_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_constraint_network_scaling.py"
set "DIAGNOSTIC_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_constraint_network_diagnostics.py"
set "CN_DIAGNOSTICS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CN_DIAGNOSTICS_DIR%"
set "CN_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CN_PROBLEM_DIR%"
set "CN_PROBLEM_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%CN_PROBLEM_PLOTS_DIR%"
set "PROBLEM_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_constraint_network_problem.py"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found:
    echo   %GPU_ALGOBENCH%
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%EXPORT_CONSTRAINT_NETWORK%" (
    echo ERROR: export_constraint_network.exe was not found:
    echo   %EXPORT_CONSTRAINT_NETWORK%
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
if "%OVERWRITE_RESULTS%"=="1" if exist "%CN_JSONL%" del "%CN_JSONL%"

set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Constraint-network candidate-count sweep
    echo ============================================================
    echo Output JSONL:
    echo   %CN_JSONL%
    echo.

    REM ------------------------------------------------------------
    REM Fine-grained sweep. The benchmark presets are still useful for
    REM defaults, but this runner passes explicit sizes so the plots show
    REM the CPU/GPU crossover region with more resolution.
    REM ------------------------------------------------------------

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset tiny --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=64 --set resources=32 --set candidates=4096 --set sweep_label=cn_4k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset tiny --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=128 --set resources=64 --set candidates=16384 --set sweep_label=cn_16k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset tiny --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=256 --set resources=128 --set candidates=65536 --set sweep_label=cn_64k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset tiny --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=384 --set resources=192 --set candidates=131072 --set sweep_label=cn_128k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset small --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=512 --set resources=256 --set candidates=262144 --set sweep_label=cn_256k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset small --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=1024 --set resources=512 --set candidates=524288 --set sweep_label=cn_512k --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset medium --repeat %CN_REPEAT% --warmup %CN_WARMUP% --set tasks=2048 --set resources=1024 --set candidates=1048576 --set sweep_label=cn_1m --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset medium --repeat 3 --warmup %CN_WARMUP% --set tasks=4096 --set resources=1024 --set candidates=2097152 --set sweep_label=cn_2m --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    "%GPU_ALGOBENCH%" --benchmark constraint_network --preset large --repeat 3 --warmup %CN_WARMUP% --set tasks=8192 --set resources=2048 --set candidates=4194304 --set sweep_label=cn_4m --output "%CN_JSONL%"
    if errorlevel 1 exit /b 1

    if "%INCLUDE_HEAVY_CASES%"=="1" (
        "%GPU_ALGOBENCH%" --benchmark constraint_network --preset large --repeat 2 --warmup %CN_WARMUP% --set tasks=16384 --set resources=4096 --set candidates=8388608 --set sweep_label=cn_8m --output "%CN_JSONL%"
        if errorlevel 1 exit /b 1

        "%GPU_ALGOBENCH%" --benchmark constraint_network --preset large --repeat 1 --warmup %CN_WARMUP% --set tasks=32768 --set resources=8192 --set candidates=16777216 --set sweep_label=cn_16m --output "%CN_JSONL%"
        if errorlevel 1 exit /b 1
    )
)

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze constraint-network JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%CN_JSONL%" --output-dir "%CN_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot constraint-network scaling results
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --jsonl "%CN_JSONL%" --output-dir "%CN_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1

    echo.
    echo ============================================================
    echo STEP 4: Plot constraint-network diagnostics
    echo ============================================================
    "%PYTHON%" "%DIAGNOSTIC_PLOT_SCRIPT%" --jsonl "%CN_JSONL%" --output-dir "%CN_DIAGNOSTICS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

if "%RUN_PROBLEM_EXPORT%"=="1" (
    echo.
    echo ============================================================
    echo STEP 5: Export and plot one inspectable constraint problem definition
    echo ============================================================
    "%EXPORT_CONSTRAINT_NETWORK%" --tasks %CN_PROBLEM_TASKS% --resources %CN_PROBLEM_RESOURCES% --candidates %CN_PROBLEM_CANDIDATES% --output-dir "%CN_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1

    "%PYTHON%" "%PROBLEM_PLOT_SCRIPT%" --input-dir "%CN_PROBLEM_PATH%" --output-dir "%CN_PROBLEM_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo CONSTRAINT NETWORK SWEEP COMPLETE
echo ============================================================
echo JSONL:
echo   %CN_JSONL%
echo Analysis:
echo   %CN_ANALYSIS_PATH%
echo Plots:
echo   %CN_PLOTS_PATH%
echo Problem definition export:
echo   %CN_PROBLEM_PATH%
echo Problem definition plots:
echo   %CN_PROBLEM_PLOTS_PATH%

echo.
endlocal
