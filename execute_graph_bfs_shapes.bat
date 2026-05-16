@echo off
setlocal EnableExtensions

REM ============================================================
REM Graph BFS shape-comparison runner
REM Change these values at the top of the file.
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "BFS_PRESET=small"
set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RESULTS_DIR=results"
set "RESULTS_FILE=graph_bfs_shape_comparison.jsonl"

REM Set to 1 to include GPU rows when the binary was built with CUDA.
REM Set to 0 to force CPU-only rows.
set "RUN_GPU=1"

REM Set to 1 to delete the previous JSONL file before running.
REM Set to 0 to append to the existing file.
set "OVERWRITE_RESULTS=1"

REM ============================================================
REM Path detection
REM ============================================================

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "RESULTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%RESULTS_FILE%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: Could not find gpu_algobench.exe.
    echo Expected path:
    echo   %GPU_ALGOBENCH%
    echo.
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    exit /b 1
)

if not exist "%PROJECT_DIR%\%RESULTS_DIR%" (
    mkdir "%PROJECT_DIR%\%RESULTS_DIR%" >nul 2>nul
)

if "%OVERWRITE_RESULTS%"=="1" (
    if exist "%RESULTS_PATH%" del "%RESULTS_PATH%"
)

set "GPU_FLAG="
if "%RUN_GPU%"=="0" set "GPU_FLAG=--no-gpu"

REM ============================================================
REM Summary
REM ============================================================

echo ============================================================
echo GPU Classical Algorithms Lab - Graph BFS Shape Comparison
echo ============================================================
echo Project dir:       %PROJECT_DIR%
echo Executable dir:    %EXE_DIR%
echo Preset/size:       %BFS_PRESET%
echo Repeat:            %BFS_REPEAT%
echo Warmup:            %BFS_WARMUP%
echo RUN_GPU:           %RUN_GPU%
echo Results:           %RESULTS_PATH%
echo ============================================================
echo.
echo NOTE: chain is intentionally GPU-hostile because its frontier is one node wide.
echo NOTE: set BFS_PRESET=large manually for a heavier study, but large chain runs can be slow.
echo.

call :run_shape chain
if errorlevel 1 exit /b 1

call :run_shape grid
if errorlevel 1 exit /b 1

call :run_shape layered
if errorlevel 1 exit /b 1

call :run_shape random
if errorlevel 1 exit /b 1

echo.
echo ============================================================
echo GRAPH BFS SHAPE COMPARISON FINISHED
echo Results written to:
echo   %RESULTS_PATH%
echo ============================================================
exit /b 0

:run_shape
set "GRAPH_SHAPE=%~1"
echo ------------------------------------------------------------
echo GRAPH BFS SHAPE: %GRAPH_SHAPE%
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark graph_bfs --preset %BFS_PRESET% --repeat %BFS_REPEAT% --warmup %BFS_WARMUP% --set graph=%GRAPH_SHAPE% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark graph_bfs --preset "%BFS_PRESET%" --repeat "%BFS_REPEAT%" --warmup "%BFS_WARMUP%" --set graph=%GRAPH_SHAPE% %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: graph_bfs shape %GRAPH_SHAPE%
    exit /b 1
)
echo.
exit /b 0
