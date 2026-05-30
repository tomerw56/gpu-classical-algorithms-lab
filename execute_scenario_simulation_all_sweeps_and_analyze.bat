@echo off
setlocal EnableExtensions

REM ============================================================
REM Scenario simulation all-in-one sweep + analysis + plotting flow
REM ============================================================

set "PYTHON=python"
set "BUILD_DIR=build-cuda-ninja"
set "RUN_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "RUN_PROBLEM_EXPORT=1"
set "PLOT_SHOW=0"
set "SC_REPEAT=5"
set "SC_WARMUP=1"
set "INCLUDE_HEAVY_CASES=0"
set "OVERWRITE_RESULTS=1"

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

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "EXPORT_EXE=%EXE_DIR%\export_scenario_simulation.exe"
set "SC_JSONL=%PROJECT_DIR%\%RESULTS_DIR%\%SC_RESULTS_FILE%"
set "SC_ANALYSIS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_ANALYSIS_DIR%"
set "SC_PLOTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_PLOTS_DIR%"
set "SC_PROBLEM_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%SC_PROBLEM_DIR%"
set "ANALYZE_SCRIPT=%PROJECT_DIR%\scripts\analyze_scenario_simulation_jsonl.py"
set "PLOT_SCRIPT=%PROJECT_DIR%\scripts\plot_scenario_simulation_scaling.py"
set "PLOT_PROBLEM_SCRIPT=%PROJECT_DIR%\scripts\plot_scenario_simulation_problem.py"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: gpu_algobench.exe was not found: %GPU_ALGOBENCH%
    echo Build first: configure_ninja_cuda128.bat
    exit /b 1
)
if not exist "%PROJECT_DIR%\%RESULTS_DIR%" mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
if "%OVERWRITE_RESULTS%"=="1" if exist "%SC_JSONL%" del "%SC_JSONL%"
set "PLOT_SHOW_ARG="
if "%PLOT_SHOW%"=="1" set "PLOT_SHOW_ARG=--show"

if "%RUN_SWEEP%"=="1" (
    echo.
    echo ============================================================
    echo STEP 1: Scenario-count sweep
    echo ============================================================
    echo Output JSONL:
    echo   %SC_JSONL%
    echo.
    call :run_case sc_4k 32 16 4096
    call :run_case sc_16k 32 16 16384
    call :run_case sc_64k 64 32 65536
    call :run_case sc_256k 64 32 262144
    call :run_case sc_1m 128 64 1048576
    call :run_case sc_4m 128 64 4194304
    if "%INCLUDE_HEAVY_CASES%"=="1" (
        REM Optional stress point. sc_16m was intentionally removed: it was too slow
        REM for the amount of additional evidence it provided.
        call :run_case sc_8m 128 64 8388608
    )
)

if "%RUN_ANALYSIS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 2: Analyze scenario JSONL
    echo ============================================================
    "%PYTHON%" "%ANALYZE_SCRIPT%" --jsonl "%SC_JSONL%" --output-dir "%SC_ANALYSIS_PATH%"
    if errorlevel 1 exit /b 1
)

if "%RUN_PLOTS%"=="1" (
    echo.
    echo ============================================================
    echo STEP 3: Plot scenario scaling
    echo ============================================================
    "%PYTHON%" "%PLOT_SCRIPT%" --summary-csv "%SC_ANALYSIS_PATH%\scenario_simulation_analysis_summary.csv" --output-dir "%SC_PLOTS_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

if "%RUN_PROBLEM_EXPORT%"=="1" (
    echo.
    echo ============================================================
    echo STEP 4: Export and plot inspectable scenario problem
    echo ============================================================
    if not exist "%EXPORT_EXE%" (
        echo ERROR: export_scenario_simulation.exe was not found: %EXPORT_EXE%
        exit /b 1
    )
    "%EXPORT_EXE%" --tasks %SC_PROBLEM_TASKS% --resources %SC_PROBLEM_RESOURCES% --scenarios %SC_PROBLEM_SCENARIOS% --output-dir "%SC_PROBLEM_PATH%"
    if errorlevel 1 exit /b 1
    "%PYTHON%" "%PLOT_PROBLEM_SCRIPT%" --input-dir "%SC_PROBLEM_PATH%" %PLOT_SHOW_ARG%
    if errorlevel 1 exit /b 1
)

echo.
echo ============================================================
echo SCENARIO SIMULATION SWEEP COMPLETE
echo Results:
echo   %SC_JSONL%
echo   %SC_ANALYSIS_PATH%
echo   %SC_PLOTS_PATH%
echo   %SC_PROBLEM_PATH%
echo ============================================================
exit /b 0

:run_case
echo Running %~1: tasks=%~2 resources=%~3 scenarios=%~4
"%GPU_ALGOBENCH%" --benchmark scenario_simulation --preset tiny --repeat %SC_REPEAT% --warmup %SC_WARMUP% --set sweep_label=%~1 --set tasks=%~2 --set resources=%~3 --set scenarios=%~4 --output "%SC_JSONL%"
if errorlevel 1 exit /b 1
exit /b 0
