@echo off
setlocal EnableExtensions

REM ============================================================
REM User-tunable settings
REM Change these values once at the top of the file.
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "BENCHMARK_NAME=all"
set "BENCHMARK_PRESET=small"
set "BENCHMARK_REPEAT=5"
set "BENCHMARK_WARMUP=1"
set "RESULTS_DIR=results"
set "RESULTS_FILE=execute_all_tests.jsonl"

REM Set to 1 to include GPU benchmark variants when the binary was built with CUDA.
REM Set to 0 to force CPU-only benchmark execution.
set "RUN_GPU=1"

REM Set to 1 to delete the previous JSONL file before running.
REM Set to 0 to append to the existing file.
set "OVERWRITE_RESULTS=1"

REM ============================================================
REM Path detection
REM ============================================================

REM %~dp0 always ends with a trailing backslash. That is convenient for
REM concatenation, but dangerous when passing quoted paths to some tools.
REM In particular, ctest --test-dir "path\" can be parsed as one long
REM argument that accidentally includes the next flag.
REM
REM Therefore all directory variables below are normalized WITHOUT a trailing
REM backslash. When building file paths, always insert the backslash explicitly.

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

REM The script is designed to work from the repository root.
REM CMake may also copy it into the build folder; if run from there, detect that.
if exist "%SCRIPT_DIR%\gpu_algobench.exe" (
    set "EXE_DIR=%SCRIPT_DIR%"
    for %%I in ("%SCRIPT_DIR%\..") do set "PROJECT_DIR=%%~fI"
) else (
    set "PROJECT_DIR=%SCRIPT_DIR%"
    set "EXE_DIR=%SCRIPT_DIR%\%BUILD_DIR%"
)

set "GPU_ALGOBENCH=%EXE_DIR%\gpu_algobench.exe"
set "LIST_BENCHMARKS=%EXE_DIR%\list_benchmarks.exe"
set "VALIDATE_ALL=%EXE_DIR%\validate_all.exe"
set "CUDA_PROBE=%EXE_DIR%\cuda_probe.exe"
set "RESULTS_PATH=%PROJECT_DIR%\%RESULTS_DIR%\%RESULTS_FILE%"

if not exist "%GPU_ALGOBENCH%" (
    echo ERROR: Could not find gpu_algobench.exe.
    echo Expected path:
    echo   %GPU_ALGOBENCH%
    echo.
    echo Build first, for example:
    echo   configure_ninja_cuda128.bat
    echo or:
    echo   cmake --build %BUILD_DIR%
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
echo GPU Classical Algorithms Lab - Execute All Tests
echo ============================================================
echo Project dir:       %PROJECT_DIR%
echo Executable dir:    %EXE_DIR%
echo Benchmark:         %BENCHMARK_NAME%
echo Preset/size:       %BENCHMARK_PRESET%
echo Repeat:            %BENCHMARK_REPEAT%
echo Warmup:            %BENCHMARK_WARMUP%
echo RUN_GPU:           %RUN_GPU%
echo Results:           %RESULTS_PATH%
echo ============================================================

REM ============================================================
REM 1. List benchmarks
REM ============================================================

echo.
echo ------------------------------------------------------------
echo TEST 1: list_benchmarks
echo COMMAND: "%LIST_BENCHMARKS%"
echo ------------------------------------------------------------
"%LIST_BENCHMARKS%"
if errorlevel 1 (
    echo FAILED: list_benchmarks
    exit /b 1
)

REM ============================================================
REM 2. CUDA runtime probe
REM ============================================================

if exist "%CUDA_PROBE%" (
    echo.
    echo ------------------------------------------------------------
    echo TEST 2: cuda_probe
    echo COMMAND: "%CUDA_PROBE%"
    echo ------------------------------------------------------------
    "%CUDA_PROBE%"
    if errorlevel 1 (
        if "%RUN_GPU%"=="1" (
            echo FAILED: cuda_probe
            exit /b 1
        ) else (
            echo WARNING: cuda_probe reported CUDA unavailable, but RUN_GPU=0, so continuing.
        )
    )
) else (
    echo.
    echo ------------------------------------------------------------
    echo TEST 2: cuda_probe
    echo SKIPPED: cuda_probe.exe was not built.
    echo ------------------------------------------------------------
)

REM ============================================================
REM 3. CTest unit/smoke tests
REM ============================================================

echo.
echo ------------------------------------------------------------
echo TEST 3: ctest
echo COMMAND: ctest --test-dir "%EXE_DIR%" -N
echo ------------------------------------------------------------
ctest --test-dir "%EXE_DIR%" -N
if errorlevel 1 (
    echo FAILED: ctest test discovery
    exit /b 1
)
echo.
echo COMMAND: ctest --test-dir "%EXE_DIR%" --output-on-failure --verbose
echo ------------------------------------------------------------
ctest --test-dir "%EXE_DIR%" --output-on-failure --verbose
if errorlevel 1 (
    echo FAILED: ctest
    exit /b 1
)

REM ============================================================
REM 4. validate_all executable
REM ============================================================

echo.
echo ------------------------------------------------------------
echo TEST 4: validate_all
echo COMMAND: "%VALIDATE_ALL%"
echo ------------------------------------------------------------
"%VALIDATE_ALL%"
if errorlevel 1 (
    echo FAILED: validate_all
    exit /b 1
)

REM ============================================================
REM 5. Full benchmark run
REM ============================================================

echo.
echo ------------------------------------------------------------
echo TEST 5: gpu_algobench benchmark run
echo COMMAND: "%GPU_ALGOBENCH%" --benchmark %BENCHMARK_NAME% --preset %BENCHMARK_PRESET% --repeat %BENCHMARK_REPEAT% --warmup %BENCHMARK_WARMUP% %GPU_FLAG% --output "%RESULTS_PATH%"
echo ------------------------------------------------------------
"%GPU_ALGOBENCH%" --benchmark "%BENCHMARK_NAME%" --preset "%BENCHMARK_PRESET%" --repeat "%BENCHMARK_REPEAT%" --warmup "%BENCHMARK_WARMUP%" %GPU_FLAG% --output "%RESULTS_PATH%"
if errorlevel 1 (
    echo FAILED: gpu_algobench benchmark run
    exit /b 1
)

echo.
echo ============================================================
echo ALL TESTS PASSED
echo Results written to:
echo   %RESULTS_PATH%
echo ============================================================

endlocal
