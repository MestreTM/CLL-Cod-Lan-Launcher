@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Build LanLauncherQt against a static Qt prefix (MinGW Makefiles).
rem Ninja is avoided here: it loops when the source is on C: and Qt is on Y:.

rem --- edit these if your Qt / MinGW live somewhere else ---
set "PREFIX=Y:\QT\6.11.2-static"
set "MINGW_BIN=Y:\QT\Tools\mingw1310_64\bin"
set "BUILD_DIR=build-static"
set "DIST=dist-static"

cd /d "%~dp0\.."

if not exist "CMakeLists.txt" (
    echo [ERROR] Run this from the LanLauncher tree. Current dir: %CD%
    exit /b 1
)
if not exist "%PREFIX%\bin\qmake.exe" (
    echo [ERROR] %PREFIX%\bin\qmake.exe is missing. Run scripts\build-qt-static.bat first.
    exit /b 1
)
if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo [ERROR] mingw32-make.exe not found in %MINGW_BIN%
    exit /b 1
)

set "PATH=%MINGW_BIN%;%PREFIX%\bin;%PATH%"

echo.
echo === LanLauncherQt static Qt / MinGW Makefiles ===
echo PREFIX %PREFIX%
echo SRC    %CD%
echo.

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cmake -S . -B "%BUILD_DIR%" -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%PREFIX%" ^
    -DCMAKE_MAKE_PROGRAM="%MINGW_BIN%\mingw32-make.exe" ^
    -DCMAKE_CXX_COMPILER="%MINGW_BIN%\g++.exe"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%\CodLanLaucher.exe" (
    echo [ERROR] CodLanLaucher.exe was not produced
    dir /s /b "%BUILD_DIR%\*.exe"
    exit /b 1
)

if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy /y "%BUILD_DIR%\CodLanLaucher.exe" "%DIST%\CodLanLaucher.exe" >nul

echo.
echo Done: %CD%\%DIST%\CodLanLaucher.exe
dir /b "%DIST%"
echo.
exit /b 0
