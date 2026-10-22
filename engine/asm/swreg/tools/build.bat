@echo off
rem Assemble src\swreg.asm (UASM), patch the MZ header, and stage a runnable dir.
rem
rem   build.bat [variant]      oracle (default) ^| serial ^| file ^| tee ^| log ^| fast
rem
rem Output: <repo>\build\swreg\<variant>\ - the exe plus the minimum game files
rem from _ref\orig-bin.  build.sh documents what the variants mean.
setlocal
set VARIANT=%~1
if "%VARIANT%"=="" set VARIANT=oracle
set KNOWN=
if /I "%VARIANT%"=="oracle" set KNOWN=1
if /I "%VARIANT%"=="serial" set KNOWN=1
if /I "%VARIANT%"=="file" set KNOWN=1
if /I "%VARIANT%"=="tee" set KNOWN=1
if /I "%VARIANT%"=="log" set KNOWN=1
if not defined KNOWN (
    echo build: unknown variant %VARIANT% ^(oracle, serial, file, tee, log, fast^)
    exit /b 2
)
set DEFS=
if /I "%VARIANT%"=="serial" set DEFS=-DSEAM
if /I "%VARIANT%"=="file" set DEFS=-DSEAM -DSEAM_FILE
if /I "%VARIANT%"=="tee" set DEFS=-DSEAM -DSEAM_FILE -DSEAM_TEE
if /I "%VARIANT%"=="log" set DEFS=-DSEAM -DSEAM_FILE -DSEAM_LOG
if /I "%VARIANT%"=="fast" set DEFS=-DSEAM -DSEAM_FILE -DSEAM_FAST
set OUT=..\..\..\..\build\swreg\%VARIANT%
set REFBIN=..\..\..\..\_ref\orig-bin
if not exist %OUT% mkdir %OUT%
uasm -nologo -mz %DEFS% -Fo=%OUT%\SWREG.EXE ..\src\swreg.asm
if errorlevel 1 exit /b 1
python fixhdr.py %OUT%\SWREG.EXE
if errorlevel 1 exit /b 1
if /I "%VARIANT%"=="oracle" (
    certutil -hashfile %OUT%\SWREG.EXE MD5
) else (
    python check_mz_layout.py %OUT%\SWREG.EXE
    if errorlevel 1 exit /b 1
)
for %%F in (CH2.BAT SBFMDRV.COM WORLD.DAT PICTURES.VGA CURGAME) do copy %REFBIN%\%%F %OUT%\%%F >NUL
echo build: %VARIANT% -^> %OUT%
