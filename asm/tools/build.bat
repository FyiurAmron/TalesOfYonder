@echo off
rem Assemble src\swreg.asm (UASM), patch the MZ header, and stage a runnable dir.
rem Outputs go to <repo>\build\uasm\ - the exe plus the minimum game files from _ref\orig-bin.
set OUT=..\..\build\uasm
set REFBIN=..\..\_ref\orig-bin
if not exist ..\..\build mkdir ..\..\build
if not exist %OUT% mkdir %OUT%
uasm -nologo -mz -Fo=%OUT%\SWREG.EXE ..\src\swreg.asm
if errorlevel 1 exit /b 1
python fixhdr.py %OUT%\SWREG.EXE
certutil -hashfile %OUT%\SWREG.EXE MD5
for %%F in (CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME) do copy %REFBIN%\%%F %OUT%\%%F >NUL
