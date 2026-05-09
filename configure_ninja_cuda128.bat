@echo off
setlocal

REM --- Adjust this if your VS path is different ---
set VS_VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

REM --- CUDA 12.8 path ---
set CUDA_ROOT=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8

call %VS_VCVARS%

echo.
echo === Tool checks ===
where cl
where link
where rc
where mt
where ninja
"%CUDA_ROOT%\bin\nvcc.exe" --version

echo.
echo === Cleaning build folder ===
if exist build-cuda-ninja rmdir /s /q build-cuda-ninja

echo.
echo === Configuring with Ninja + MSVC + CUDA 12.8 ===
cmake -S . -B build-cuda-ninja -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DENABLE_CUDA=ON ^
  -DCMAKE_C_COMPILER=cl.exe ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CUDA_COMPILER="%CUDA_ROOT%\bin\nvcc.exe" ^
  -DCMAKE_CUDA_HOST_COMPILER=cl.exe ^
  -DCUDAToolkit_ROOT="%CUDA_ROOT%" ^
  -DCMAKE_CUDA_ARCHITECTURES=89

if errorlevel 1 exit /b 1

echo.
echo === Building ===
cmake --build build-cuda-ninja

if errorlevel 1 exit /b 1

echo.
echo === Running benchmark ===
build-cuda-ninja\gpu_algobench.exe --benchmark foundation_smoke --preset small --repeat 5

endlocal