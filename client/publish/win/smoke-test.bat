@echo off
rem No DOSBox-X needed: replays the bundled timed BIN stream in a window (which also
rem checks client\SDL3.dll and the renderer), reports the decode stats and
rem writes sample\smoke.ppm (the last frame).  Close the window to finish.
setlocal
"%~dp0client\Yt2Client.exe" --file "%~dp0sample\STREAM.BIN" --stats 0 --ppm "%~dp0sample\smoke.ppm" --hold
echo.
echo exit code %ERRORLEVEL%  (0 = ok)
pause
