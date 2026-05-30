@echo off
setlocal EnableExtensions

REM ============================================================
REM Scenario simulation plotting-only flow
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "PLOT_SHOW=0"
set "RUN_ANALYSIS=1"
set "RUN_PROBLEM_EXPORT=1"

set "RESULTS_DIR=results"
set "SC_RESULTS_FILE=scenario_simulation_scale_sweep.jsonl"
set "SC_ANALYSIS_DIR=scenario_simulation_analysis"
set "SC_PLOTS_DIR=scenario_simulation_plots"
set "SC_PROBLEM_DIR=scenario_simulation_problem_demo"

set "SC_PROBLEM_TASKS=32"
set "SC_PROBLEM_RESOURCES=16"
set "SC_PROBLEM_SCENARIOS=2048"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "EXPORT_EXE=%EXE_DIR%\export_scenario_simulation.exe"
set "SC_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%SC_RESULTS_FILE%"
set "SC_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_ANALYSIS_DIR%"
set "SC_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_PLOTS_DIR%"
set "SC_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_PROBLEM_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_scenario_simulation_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_scenario_simulation_scaling.py"
set "PLOT_PROBLEM_SCRIPT=%PROJECT_DIR%\scripts\plot_scenario_simulation_problem.py"
set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

if not exist "%SC_JSONL%" (
    echo ERROR: Scenario JSONL was not found:
    echo   %SC_JSONL%
    echo Run first:
    echo   execute_scenario_simulation_all_sweeps_and_analyze.bat
    exit /b 1
)

if "%RUN_ANALYSIS%"=="1" (
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%SC_JSONL%" --output-dir "%SC_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

"%PYTHON%" "%PLOT_SCRIPT%" --summary-csv "%SC_ANALYSIS_PATH%\scenario_simulation_analysis_summary.csv" --output-dir "%SC_PLOTS_PATH%" %PLOT_SHOW_ARG%
if errorlevel 1 exit /b 1

if "%RUN_PROBLEM_EXPORT%"=="1" (
    if not exist "%EXPORT_EXE%" (
        echo ERROR: export_scenario_simulation.exe was not found: %EXPORT_EXE%
        exit /b 1
    )
    "%EXPORT_EXE%" --tasks %SC_PROBLEM_TASKS% --resources %SC_PROBLEM_RESOURCES% --scenarios %SC_PROBLEM_SCENARIOS% --output-dir "%SC_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
)

"%PYTHON%" "%PLOT_PROBLEM_SCRIPT%" --input-dir "%SC_PROBLEM_PATH%" %PLOT_SHOW_ARG%
if errorlevel 1 exit /b 1

echo.
echo SCENARIO SIMULATION PLOTTING COMPLETE
echo   %SC_ANALYSIS_PATH%
echo   %SC_PLOTS_PATH%
echo   %SC_PROBLEM_PATH%
endlocal
