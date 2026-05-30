@echo off
setlocal EnableExtensions

REM ============================================================
REM Local-search move plotting-only flow
REM
REM Use this when benchmark results already exist and you only want to
REM regenerate analysis CSVs, scaling plots, and the inspectable problem plots.
REM It does not run the expensive benchmark sweep.
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "PLOT_SHOW=0"
set "RUN_PROBLEM_EXPORT=1"
set "RUN_ANALYSIS=1"

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
    REM Script was copied into the build directory by CMake.
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    REM Script is being run from the repository root, the canonical mode.
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "EXPORTER=%EXE_DIR%\export_local_search_moves.exe"
set "LS_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%LS_RESULTS_FILE%"
set "LS_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_ANALYSIS_DIR%"
set "LS_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_PLOTS_DIR%"
set "LS_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%LS_PROBLEM_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_local_search_moves_jsonl.py"
set "SCALING_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_local_search_moves_scaling.py"
set "PROBLEM_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_local_search_moves_problem.py"

set "SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "SHOW_ARG=--show"

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul

echo.
echo ============================================================
echo STEP 1: Local-search move scaling plots
echo ============================================================
echo Input JSONL:
echo   %LS_JSONL%
echo.

if not exist "%LS_JSONL%" (
    echo ERROR: Local-search move JSONL was not found.
    echo Expected:
    echo   %LS_JSONL%
    echo.
    echo Run the benchmark sweep first:
    echo   execute_local_search_moves_all_sweeps_and_analyze.bat
    exit /b 1
)

if "%RUN_ANALYSIS%"=="1" (
    echo Running analysis...
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%LS_JSONL%" --output-dir "%LS_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if not exist "%LS_ANALYSIS_PATH%\local_search_moves_analysis_summary.csv" (
    echo ERROR: Analysis summary CSV was not found:
    echo   %LS_ANALYSIS_PATH%\local_search_moves_analysis_summary.csv
    echo Set RUN_ANALYSIS=1 or run the full sweep first.
    exit /b 1
)

echo Plotting scaling results...
"%PYTHON%" "%SCALING_PLOT_SCRIPT%" --summary-csv "%LS_ANALYSIS_PATH%\local_search_moves_analysis_summary.csv" --output-dir "%LS_PLOTS_PATH%" %SHOW_ARG%
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo STEP 2: Local-search problem-definition plots
echo ============================================================
echo Problem directory:
echo   %LS_PROBLEM_PATH%
echo.

if "%RUN_PROBLEM_EXPORT%"=="1" (
    if not exist "%EXPORTER%" (
        echo ERROR: export_local_search_moves.exe was not found:
        echo   %EXPORTER%
        echo Build first, for example:
        echo   configure_ninja_cuda128.bat
        exit /b 1
    )
    echo Exporting inspectable local-search instance...
    "%EXPORTER%" --tasks %LS_PROBLEM_TASKS% --resources %LS_PROBLEM_RESOURCES% --moves %LS_PROBLEM_MOVES% --output-dir "%LS_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
)

if not exist "%LS_PROBLEM_PATH%\candidate_moves.csv" (
    echo ERROR: Problem export files were not found in:
    echo   %LS_PROBLEM_PATH%
    echo Set RUN_PROBLEM_EXPORT=1 or run the full local-search sweep first.
    exit /b 1
)

echo Plotting problem definition...
"%PYTHON%" "%PROBLEM_PLOT_SCRIPT%" --input-dir "%LS_PROBLEM_PATH%" %SHOW_ARG%
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo LOCAL-SEARCH PLOTTING COMPLETE
echo Outputs:
echo   %LS_ANALYSIS_PATH%
echo   %LS_PLOTS_PATH%
echo   %LS_PROBLEM_PATH%
echo ============================================================

endlocal
