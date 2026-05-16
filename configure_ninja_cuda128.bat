@echo off
setlocal EnableExtensions

REM ============================================================
REM User-tunable settings
REM ============================================================

set "BUILD_DIR=build-cuda-ninja"
set "CUDA_ROOT=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"
set "CUDA_ARCHITECTURES=89"
set "BUILD_TYPE=Release"

REM Adjust this if Visual Studio is installed somewhere else.
set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

REM ============================================================
REM Initialize Visual Studio build environment
REM ============================================================

if not exist "%VS_VCVARS%" (
    echo ERROR: Could not find vcvars64.bat.
    echo Expected:
    echo   %VS_VCVARS%
    exit /b 1
)

call "%VS_VCVARS%"
if errorlevel 1 exit /b 1

set "CUDA_PATH=%CUDA_ROOT%"
set "CUDACXX=%CUDA_ROOT%\bin\nvcc.exe"
set "PATH=%CUDA_ROOT%\bin;%PATH%"

REM ============================================================
REM Tool checks
REM ============================================================

echo.
echo === Tool checks ===
where cl || exit /b 1
where link || exit /b 1
where rc || exit /b 1
where mt || exit /b 1
where ninja || exit /b 1
"%CUDA_ROOT%\bin\nvcc.exe" --version || exit /b 1

REM ============================================================
REM Configure and build
REM ============================================================

echo.
echo === Cleaning build folder ===
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo.
echo === Configuring with Ninja + MSVC + CUDA %CUDA_ROOT% ===
cmake -S . -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
  -DENABLE_CUDA=ON ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CUDA_COMPILER="%CUDA_ROOT%\bin\nvcc.exe" ^
  -DCMAKE_CUDA_HOST_COMPILER=cl.exe ^
  -DCMAKE_CUDA_ARCHITECTURES=%CUDA_ARCHITECTURES% ^
  -DCUDAToolkit_ROOT="%CUDA_ROOT%"

if errorlevel 1 exit /b 1

echo.
echo === Building ===
cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo.
echo === Listing tests ===
ctest --test-dir "%BUILD_DIR%" -N
if errorlevel 1 exit /b 1

echo.
echo === Done ===
echo Build directory: %BUILD_DIR%
echo Run all tests with:
echo   execute_all_tests.bat

endlocal
