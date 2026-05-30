@echo off
setlocal EnableExtensions

REM ============================================================
REM Local-search move evaluation all-in-one sweep + analysis + plotting
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "RUN_PROBLEM_EXPORT=1"
set "PLOT_SHOW=0"
set "LS_REPEAT=5"
set "LS_WARMUP=1"
set "INCLUDE_HEAVY_CASES=1"
set "OVERWRITE_RESULTS=1"

set "RESULTS_DIR=results"
set "LS_RESULTS_FILE=local_search_moves_scale_sweep.jsonl"
set "LS_ANALYSIS_DIR=local_search_moves_analysis"
set "LS_PLOTS_DIR=local_search_moves_plots"
set "LS_PROBLEM_DIR=local_search_moves_problem_demo"

set "LS_PROBLEM_TASKS=32"
set "LS_PROBLEM_RESOURCES=24"
set "LS_PROBLEM_MOVES=1024"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "EXPORT_EXE=%EXE_DIR%\export_local_search_moves.exe"
set "LS_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%LS_RESULTS_FILE%"
set "LS_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_ANALYSIS_DIR%"
set "LS_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_PLOTS_DIR%"
set "LS_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_PROBLEM_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_local_search_moves_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_local_search_moves_scaling.py"
set "PLOT_PROBLEM_SCRIPT=%PROJECT_DIR%\scripts\plot_local_search_moves_problem.py"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found: %GPU_ALGOBENCH%
    echo Build first: configure_ninja_cuda128.bat
    exit /b 1
)
if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
if "%OVERWRITE_RESULTS%"=="1" if exist "%LS_JSONL%" del "%LS_JSONL%"
set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Local-search move-count sweep
    echo ============================================================
    echo Output JSONL:
    echo   %LS_JSONL%
    echo.
    call :run_case ls_4k 128 256 4096
    call :run_case ls_16k 256 512 16384
    call :run_case ls_64k 512 1024 65536
    call :run_case ls_256k 1024 1024 262144
    call :run_case ls_1m 2048 2048 1048576
    call :run_case ls_4m 4096 4096 4194304
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        call :run_case ls_8m 4096 4096 8388608
        call :run_case ls_16m 8192 4096 16777216
    )
)

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze local-search JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%LS_JSONL%" --output-dir "%LS_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot local-search scaling
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --summary-csv "%LS_ANALYSIS_PATH%\local_search_moves_analysis_summary.csv" --output-dir "%LS_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

if "%RUN_PROBLEM_EXPORT%"=="1" (
    echo.
    echo ============================================================
    echo STEP 4: Export and plot inspectable local-search problem
    echo ============================================================
    if not exist "%EXPORT_EXE%" (
        echo ERROR: export_local_search_moves.exe was not found: %EXPORT_EXE%
        exit /b 1
    )
    "%EXPORT_EXE%" --tasks %LS_PROBLEM_TASKS% --resources %LS_PROBLEM_RESOURCES% --moves %LS_PROBLEM_MOVES% --output-dir "%LS_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
    "%PYTHON%" "%PLOT_PROBLEM_SCRIPT%" --input-dir "%LS_PROBLEM_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo LOCAL-SEARCH MOVE SWEEP COMPLETE

echo Results:
echo   %LS_JSONL%
echo   %LS_ANALYSIS_PATH%
echo   %LS_PLOTS_PATH%
echo   %LS_PROBLEM_PATH%
echo ============================================================
exit /b 0

:run_case
echo Running %~1: tasks=%~2 resources=%~3 moves=%~4
"%GPU_ALGOBENCH%" --benchmark local_search_moves --preset tiny --repeat %LS_REPEAT% --warmup %LS_WARMUP% --set sweep_label=%~1 --set tasks=%~2 --set resources=%~3 --set moves=%~4 --output "%LS_JSONL%"
if errorlevel 1 exit /b 1
exit /b 0
