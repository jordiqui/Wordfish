@echo off
REM Build script for repair_exp utility using Clang or GCC (MinGW64)
REM Usage: build_repair_exp.bat [clang|gcc]

set SRC=repair_exp.cpp

if "%1"=="clang" (
    clang++ -O2 -std=c++17 -Wall -Wextra -o repair_exp.exe %SRC%
    goto end
)

if "%1"=="gcc" (
    g++ -O2 -std=c++17 -Wall -Wextra -o repair_exp.exe %SRC%
    goto end
)

echo Usage: build_repair_exp.bat [clang|gcc]

:end
