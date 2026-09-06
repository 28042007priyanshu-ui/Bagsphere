@echo off
setlocal
cd /d "%~dp0"

if not exist "C:\msys64\ucrt64\bin\gcc.exe" (
  echo MSYS2 UCRT64 compiler was not found at C:\msys64
  echo Please install MSYS2 first.
  pause
  exit /b 1
)

echo Starting BagSphere with MSYS2 UCRT64...
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"

if not exist build mkdir build
gcc -c reports_queue.c -o build\reports_queue.o || goto :error
g++ -std=c++17 server.cpp build\reports_queue.o -o build\bagsphere.exe -lws2_32 || goto :error

echo.
echo BagSphere is running at http://localhost:8000
echo.
build\bagsphere.exe
exit /b 0

:error
echo.
echo Build failed. Check the compiler output above.
pause
exit /b 1
