@echo off
setlocal enabledelayedexpansion

if "%VITASDK%"=="" (
    echo ERROR: VITASDK environment variable is not set.
    echo Please set it to your vitasdk installation path.
    echo Example: set VITASDK=C:\vitasdk
    exit /b 1
)

echo === Configuring with CMake ===
cmake -S . -B build -G "Unix Makefiles" -DVITASDK="%VITASDK%"
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed
    exit /b %ERRORLEVEL%
)

echo === Building ===
cmake --build build -- -j%NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo Build failed
    exit /b %ERRORLEVEL%
)

echo === Creating VPK ===
cmake --build build --target vita-java-me.vpk
if %ERRORLEVEL% neq 0 (
    echo VPK creation failed
    exit /b %ERRORLEVEL%
)

echo === Success! ===
echo VPK created at: build/vita-java-me.vpk
echo Install on Vita via VITA Shell or similar.
