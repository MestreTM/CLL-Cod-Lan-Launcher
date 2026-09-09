scripts/
========

Windows batch files used to build a single-file LanLauncherQt.exe against
a static Qt 6 prefix. Edit the SET lines at the top of each script if your
disks or versions differ from the defaults (Y:\QT ...).


1. build-qt-static.bat
----------------------
One-shot compile of Qt itself. Run this only when you do not already have
Y:\QT\6.11.2-static\bin\qmake.exe. Expect 1-3 hours.

  scripts\build-qt-static.bat

Variables to change (top of the file):

  QT_SHARED   Official (shared) Qt kit already installed.
              Default: Y:\QT\6.11.2\mingw_64
              Used only so qmake / cmake from that kit sit on PATH.

  MINGW_BIN   MinGW that will compile Qt and later the app.
              Default: Y:\QT\Tools\mingw1310_64\bin
              Must contain gcc.exe and mingw32-make.exe.

  NINJA_DIR   Folder with ninja.exe (Qt's configure uses it).
              Default: Y:\QT\Tools\Ninja

  QT_SRC      Unpacked qt-everywhere source tree.
              Default: Y:\src\qt-everywhere-src-6.11.2
              configure.bat must exist in that folder.

  PREFIX      Where the finished static Qt is installed.
              Default: Y:\QT\6.11.2-static
              This path must match PREFIX in build-app.bat.

  BUILD       Scratch directory for the Qt build (deleted each run).
              Default: Y:\QT\build-6.11.2-static

Needs gcc, cmake, python, perl (Strawberry) and ninja on PATH.


2. build-app.bat
----------------
Configure + compile LanLauncherQt and copy the exe to dist-static\.
Art, icons and language files are already inside the exe.

  scripts\build-app.bat

Variables to change (top of the file):

  PREFIX      Same static Qt prefix produced above.
              Default: Y:\QT\6.11.2-static

  MINGW_BIN   Same MinGW bin folder.
              Default: Y:\QT\Tools\mingw1310_64\bin

  BUILD_DIR   App build folder (wiped every run).
              Default: build-static   (next to CMakeLists.txt)

  DIST        Output folder. Only LanLauncherQt.exe is copied here.
              Default: dist-static

Uses the "MinGW Makefiles" generator on purpose. Ninja was observed to
re-run CMake forever when the repo lives on C: and Qt lives on Y:.


Typical flow
------------
  1. Install a shared Qt 6.11.2 MinGW kit (Maintenance Tool) on Y:\QT
  2. Unpack qt-everywhere-src-6.11.2 under Y:\src
  3. Edit the SET lines if your layout is different
  4. scripts\build-qt-static.bat
  5. scripts\build-app.bat
  6. Ship dist-static\LanLauncherQt.exe


Windows exe icon
----------------
resources/icons/icon.ico is linked into LanLauncherQt.exe via resources/app.rc.in.
Replace that .ico and rebuild if you want a different mark.
