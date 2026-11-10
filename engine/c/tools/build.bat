@echo off
rem Build the transliterated C engine for Windows x64 with MinGW-w64.
rem This is a thin wrapper around tools/build-mingw.sh (the same script the
rem Linux tree cross-builds with); MSVC is not used.
rem
rem Requirements on PATH: an MSYS2/MinGW shell (sh, make), python3 and uasm.
rem The game sources must already be emitted by tools/build.sh:
rem
rem   sh engine/c/tools/build.sh
rem   engine\c\tools\build.bat
rem
rem Optional: set CC to another MinGW-w64 gcc (default:
rem x86_64-w64-mingw32-gcc, falling back to a gcc whose target is mingw).
setlocal
where sh >nul 2>nul
if errorlevel 1 (
    echo build: sh.exe was not found on PATH.
    echo Run this from an MSYS2 MinGW64 shell, or use tools/build-mingw.sh.
    exit /b 1
)
sh "%~dp0build-mingw.sh" %*
exit /b %ERRORLEVEL%
