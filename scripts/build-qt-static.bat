@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Build a static Qt 6.11.2 prefix (qtbase + qtsvg only, MinGW 13.1).
rem Takes 1-3 hours. Run once; then use build-app.bat.

rem --- edit these to match your disks ---
set "QT_SHARED=Y:\QT\6.11.2\mingw_64"
set "MINGW_BIN=Y:\QT\Tools\mingw1310_64\bin"
set "NINJA_DIR=Y:\QT\Tools\Ninja"
set "QT_SRC=Y:\src\qt-everywhere-src-6.11.2"
set "PREFIX=Y:\QT\6.11.2-static"
set "BUILD=Y:\QT\build-6.11.2-static"

if not exist "%MINGW_BIN%\gcc.exe" (
    echo [ERROR] gcc not found: %MINGW_BIN%\gcc.exe
    exit /b 1
)
if not exist "%QT_SRC%\configure.bat" (
    echo [ERROR] Qt sources not found: %QT_SRC%\configure.bat
    exit /b 1
)

set "PATH=%MINGW_BIN%;%NINJA_DIR%;%QT_SHARED%\bin;%PATH%"

where gcc    >nul 2>&1 || (echo [ERROR] gcc is not on PATH & exit /b 1)
where cmake  >nul 2>&1 || (echo [ERROR] cmake is not on PATH & exit /b 1)
where python >nul 2>&1 || (echo [ERROR] python is not on PATH & exit /b 1)
where perl   >nul 2>&1 || (echo [ERROR] perl is not on PATH. Install Strawberry Perl. & exit /b 1)
where ninja  >nul 2>&1 || (echo [ERROR] ninja is not on PATH: %NINJA_DIR% & exit /b 1)

echo.
echo === Static Qt ===
echo MinGW  %MINGW_BIN%
echo Source %QT_SRC%
echo Prefix %PREFIX%
echo Build  %BUILD%
echo.

if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%"
cd /d "%BUILD%"
if errorlevel 1 (
    echo [ERROR] could not enter %BUILD%
    exit /b 1
)

call "%QT_SRC%\configure.bat" ^
    -prefix "%PREFIX%" ^
    -release -static -optimize-size ^
    -platform win32-g++ ^
    -opensource -confirm-license ^
    -nomake examples -nomake tests ^
    -submodules qtbase,qtsvg ^
    -gui -widgets ^
    -schannel -no-openssl ^
    -no-sql-mysql -no-sql-psql -no-sql-odbc ^
    -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-freetype
if errorlevel 1 (
    echo [ERROR] configure failed
    exit /b 1
)

cmake --build . --parallel
if errorlevel 1 (
    echo [ERROR] Qt compile failed
    exit /b 1
)

cmake --install .
if errorlevel 1 (
    echo [ERROR] install failed
    exit /b 1
)

if not exist "%PREFIX%\bin\qmake.exe" (
    echo [ERROR] qmake did not appear in %PREFIX%\bin
    exit /b 1
)

echo.
echo Done. Static kit:
echo   %PREFIX%
echo   qmake: %PREFIX%\bin\qmake.exe
echo.
echo Next:
echo   scripts\build-app.bat
echo.
exit /b 0
