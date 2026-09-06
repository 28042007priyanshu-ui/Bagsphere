@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ==========================================
echo        BagSphere - MSYS2 UCRT64 Build
echo ==========================================
echo.

if not exist "C:\msys64\ucrt64\bin\gcc.exe" (
    echo ERROR: MSYS2 UCRT64 GCC was not found.
    echo Install it from the MSYS2 UCRT64 terminal with:
    echo pacman -S --needed mingw-w64-ucrt-x86_64-gcc
    echo.
    pause
    exit /b 1
)

rem Put the complete MSYS2 UCRT64 runtime on PATH.
rem This lets gcc.exe find libgmp, libisl, libmpc, etc.
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"

echo Checking compiler...
gcc --version
if errorlevel 1 (
    echo GCC could not start. Check the MSYS2 installation.
    pause
    exit /b 1
)

g++ --version
if errorlevel 1 (
    echo G++ could not start. Check the MSYS2 installation.
    pause
    exit /b 1
)

if not exist "build" mkdir "build"

echo.
echo [1/2] Compiling C FIFO queue...
gcc -c reports_queue.c -o build\reports_queue.o
if errorlevel 1 (
    echo.
    echo C compilation failed.
    pause
    exit /b 1
)

echo [2/2] Compiling C++ BagSphere server...
g++ -std=c++17 server.cpp build\reports_queue.o -o build\bagsphere.exe -lws2_32 -static-libgcc -static-libstdc++
if errorlevel 1 (
    echo.
    echo C++ compilation failed.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo        BAGSPHERE IS RUNNING
echo.
echo        http://localhost:8000
echo ==========================================
echo.
echo Keep this window open while using BagSphere.
echo Press Ctrl+C to stop.
echo.

build\bagsphere.exe
pause
