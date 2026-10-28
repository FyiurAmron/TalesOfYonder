@echo off
rem Starts the server (DOSBox-X) in its own window and then the client, for a
rem one-command test.
rem
rem   play.bat            normal window
rem   play.bat --scale 4  four host pixels per game pixel (default 3)
rem
rem Closing the client window stops the client; this script then stops the
rem server it started.  Use server-start.bat + client-start.bat separately if
rem you want to watch either side's console output.
setlocal
cd /d "%~dp0"

start "YT2 server" "%~dp0server-start.bat"
rem give DOSBox-X time to create OUTPUT.BIN; the client waits for it anyway
timeout /t 2 /nobreak >nul

call "%~dp0client-start.bat" %*

rem stop the console this script started (DOSBox-X is its child)
taskkill /FI "WINDOWTITLE eq YT2 server*" /T /F >nul 2>nul
