@echo off

if /i "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
    set arch=-AARM64 -DCMAKE_GENERATOR_PLATFORM=ARM64
) else (
    set arch=-Ax64 -DCMAKE_GENERATOR_PLATFORM=x64
)

rmdir /S /Q build\CMakeFiles 2>nul
del /S /Q build\CMakeCache.txt 2>nul 1>nul

cmake -B build -DBUILD_TYPE=FLUID_DX12 %arch%
