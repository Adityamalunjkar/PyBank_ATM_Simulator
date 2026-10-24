@echo off
REM Build script for PyBank ATM Simulator (MinGW / MSYS2 g++)
where g++ >nul 2>nul
if errorlevel 1 (
    echo Error: g++ was not found on PATH.
    echo Install MinGW and add its bin folder to PATH, then run this file again.
    pause
    exit /b 1
)

g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o pybank_atm.exe
if errorlevel 1 (
    echo.
    echo Build failed. Please read the compiler errors above.
    pause
    exit /b 1
)

echo.
echo Build successful: pybank_atm.exe
echo Run it with:  pybank_atm.exe
pause
