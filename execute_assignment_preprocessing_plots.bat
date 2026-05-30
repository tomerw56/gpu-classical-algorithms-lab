@echo off
setlocal EnableExtensions

REM ============================================================
REM Assignment preprocessing plotting-only flow
REM
REM Use this when benchmark results already exist and you only want to
REM regenerate analysis CSVs, scaling plots, and the inspectable problem plots.
REM It does not run the expensive benchmark sweep.
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "PLOT_SHOW=0"

REM Set to 1 to regenerate the inspectable problem instance before plotting it.
REM Set to 0 to only plot an already-exported problem directory.
set "RUN_PROBLEM_EXPORT=1"

REM Set to 1 to run JSONL analysis before plotting scaling results.
set "RUN_ANALYSIS=1"

set "RESULTS_DIR=results"
set "AP_RESULTS_FILE=assignment_preprocessing_scale_sweep.jsonl"
set "AP_ANALYSIS_DIR=assignment_preprocessing_analysis"
set "AP_PLOTS_DIR=assignment_preprocessing_plots"
set "AP_PROBLEM_DIR=assignment_preprocessing_problem_demo"

REM Problem-demo export size. Keep this small enough to inspect visually.
set "AP_PROBLEM_TASKS=32"
set "AP_PROBLEM_RESOURCES=24"
set "AP_PROBLEM_TOP_K=4"

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

set "EXPORTER=%EXE_DIR%\export_assignment_preprocessing.exe"
set "AP_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%AP_RESULTS_FILE%"
set "AP_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_ANALYSIS_DIR%"
set "AP_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_PLOTS_DIR%"
set "AP_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%AP_PROBLEM_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_assignment_preprocessing_jsonl.py"
set "SCALING_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_assignment_preprocessing_scaling.py"
set "PROBLEM_PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_assignment_preprocessing_problem.py"

set "SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "SHOW_ARG=--show"

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul

REM ============================================================
REM Scaling analysis and plots from existing JSONL
REM ============================================================

echo.
echo ============================================================
echo STEP 1: Assignment preprocessing scaling plots
echo ============================================================
echo Input JSONL:
echo   %AP_JSONL%
echo.

if not exist "%AP_JSONL%" (
    echo ERROR: Assignment preprocessing JSONL was not found.
    echo Expected:
    echo   %AP_JSONL%
    echo.
    echo Run the benchmark sweep first:
    echo   execute_assignment_preprocessing_all_sweeps_and_analyze.bat
    exit /b 1
)

if "%RUN_ANALYSIS%"=="1" (
    echo Running analysis...
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%AP_JSONL%" --output-dir "%AP_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if not exist "%AP_ANALYSIS_PATH%\assignment_preprocessing_analysis_summary.csv" (
    echo ERROR: Analysis summary CSV was not found:
    echo   %AP_ANALYSIS_PATH%\assignment_preprocessing_analysis_summary.csv
    echo Set RUN_ANALYSIS=1 or run the full sweep first.
    exit /b 1
)

echo Plotting scaling results...
"%PYTHON%" "%SCALING_PLOT_SCRIPT%" --summary-csv "%AP_ANALYSIS_PATH%\assignment_preprocessing_analysis_summary.csv" --output-dir "%AP_PLOTS_PATH%" %SHOW_ARG%
if errorlevel 1 exit /b 1

REM ============================================================
REM Problem-definition export and plots
REM ============================================================

echo.
echo ============================================================
echo STEP 2: Assignment problem-definition plots
echo ============================================================
echo Problem directory:
echo   %AP_PROBLEM_PATH%
echo.

if "%RUN_PROBLEM_EXPORT%"=="1" (
    if not exist "%EXPORTER%" (
        echo ERROR: export_assignment_preprocessing.exe was not found:
        echo   %EXPORTER%
        echo Build first, for example:
        echo   configure_ninja_cuda128.bat
        exit /b 1
    )
    echo Exporting inspectable assignment instance...
    "%EXPORTER%" --tasks %AP_PROBLEM_TASKS% --resources %AP_PROBLEM_RESOURCES% --top-k %AP_PROBLEM_TOP_K% --output-dir "%AP_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
)

if not exist "%AP_PROBLEM_PATH%\pair_feasibility_matrix.csv" (
    echo ERROR: Problem export files were not found in:
    echo   %AP_PROBLEM_PATH%
    echo Set RUN_PROBLEM_EXPORT=1 or run the full assignment sweep first.
    exit /b 1
)

echo Plotting problem definition...
"%PYTHON%" "%PROBLEM_PLOT_SCRIPT%" --input-dir "%AP_PROBLEM_PATH%" %SHOW_ARG%
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo ASSIGNMENT PREPROCESSING PLOTTING COMPLETE
echo Outputs:
echo   %AP_ANALYSIS_PATH%
echo   %AP_PLOTS_PATH%
echo   %AP_PROBLEM_PATH%
echo ============================================================

endlocal
