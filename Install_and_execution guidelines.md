#Installation guidelines
I had to go to https://developer.nvidia.com/cuda-toolkit-archive and setup my nvidia cuda-toolkit we will call it **<CUDA_VERSION>**,make sure you do the same.
one done make sure you set the version so ```$env:CUDA_PATH="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\<CUDA_VERSION>``` this will ensure smoth compilation 

# Exectution sample
call 
```
Remove-Item -Recurse -Force build-cuda-ninja -ErrorAction SilentlyContinue
.\configure_ninja_cuda128.bat
```
**Please Note** the 128 refers to the **<CUDA_VERSION>** i had (12.8) you can totally change it the a version you have across the batch and use it.