@echo off
rem Runs the instrumented SWREG.EXE (seam build) under DOSBox-X.  The link is
rem the folder in server\game: the engine writes OUTPUT.BIN and reads
rem INPUT.BIN, and client-start.bat follows them.  Needs dosbox-x.exe on PATH
rem or in server\ (https://dosbox-x.com/).
rem
rem -nopromptfolder is not decoration: DOSBox-X otherwise asks "select a
rem working directory" on every start when its own primary config uses the
rem default "autoprompt" setting.  The profile sets the same thing, but the
rem dialog can appear before the profile is read, so it is passed here too.
setlocal
cd /d "%~dp0server"
where dosbox-x >nul 2>nul
if errorlevel 1 if not exist dosbox-x.exe (
    echo DOSBox-X not found: put dosbox-x.exe in server\ or on PATH.
    echo Download: https://dosbox-x.com/
    pause
    exit /b 1
)
if exist dosbox-x.exe (set "DOSBOX_X=%CD%\dosbox-x.exe") else (set "DOSBOX_X=dosbox-x")
echo server-start: %DATE% %TIME% running "%DOSBOX_X%" > "%CD%\start-server.log"
"%DOSBOX_X%" -nopromptfolder -fastlaunch -conf "%CD%\dosbox-x.conf"
set "RC=%ERRORLEVEL%"
echo server-start: DOSBox-X exited, errorlevel %RC% >> "%CD%\start-server.log"
if not "%RC%"=="0" (
    echo.
    echo DOSBox-X exited with code %RC%.  See server\start-server.log.
    pause
)
