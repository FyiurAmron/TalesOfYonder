@echo off
rem Runs the transliterated C engine natively - no DOSBox-X.  The link is the
rem folder in server\game: the engine writes OUTPUT.BIN and reads INPUT.BIN,
rem and client-start.bat follows them.  The engine paces itself to the guest
rem clock (3,000 cycles/ms by default; pass --cycles-per-ms N to compare with
rem another DOSBox-X speed).
setlocal
cd /d "%~dp0server"

if not exist "game\yt2game.exe" (
    echo server-start: server\game\yt2game.exe is missing; unpack the bundle again.
    pause
    exit /b 1
)

echo server-start: %DATE% %TIME% starting the C engine (folder link) > "%CD%\start-server.log"
"%~dp0server\game\yt2game.exe" --game "%~dp0server\game" --live %*
set "RC=%ERRORLEVEL%"
echo server-start: yt2game.exe exited, errorlevel %RC% >> "%CD%\start-server.log"
if not "%RC%"=="0" (
    echo.
    echo yt2game.exe exited with code %RC%.  See server\start-server.log.
    pause
)
