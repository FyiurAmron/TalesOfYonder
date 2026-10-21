@echo off
rem Follows server\game\STREAM.BIN and writes input to server\game\INPUT.BIN.
rem Start server-start.bat first (order does not matter: the client asks for a
rem keyframe when it connects).
rem Extra options go through, e.g.:  client-start.bat --scale 4 --stats 2
setlocal
"%~dp0client\Yt2Client.exe" --follow "%~dp0server\game\STREAM.BIN" --input "%~dp0server\game\INPUT.BIN" %*
if errorlevel 1 (
    echo.
    echo The client stopped with an error.
    echo If it mentioned SDL3, check that client\SDL3.dll sits next to
    echo client\Yt2Client.exe.  Run smoke-test.bat to check the window path alone.
    pause
)
