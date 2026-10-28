@echo off
rem Build the C core, the stub and the unit tests with MSVC (C11).
setlocal
set ROOT=%~dp0..\..
set BUILD=%ROOT%\build\c
if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%"

cl /nologo /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\engine\c\include" ^
  "%ROOT%\engine\c\src\cpu.c" ^
  "%ROOT%\engine\c\src\memory.c" ^
  "%ROOT%\engine\c\src\platform.c" ^
  "%ROOT%\engine\c\src\protocol.c" ^
  "%ROOT%\engine\c\src\service.c" ^
  "%ROOT%\engine\c\src\transport_folder.c" ^
  "%ROOT%\engine\c\src\transport_null.c" ^
  "%ROOT%\engine\c\src\transport_stream.c" ^
  "%ROOT%\engine\c\src\video.c" ^
  "%ROOT%\engine\c\src\stub.c" ^
  /Fe:yt2stub.exe
if errorlevel 1 goto fail

cl /nologo /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\engine\c\include" ^
  "%ROOT%\engine\c\src\cpu.c" ^
  "%ROOT%\engine\c\src\memory.c" ^
  "%ROOT%\engine\c\src\platform.c" ^
  "%ROOT%\engine\c\src\protocol.c" ^
  "%ROOT%\engine\c\src\service.c" ^
  "%ROOT%\engine\c\src\transport_folder.c" ^
  "%ROOT%\engine\c\src\transport_null.c" ^
  "%ROOT%\engine\c\src\transport_stream.c" ^
  "%ROOT%\engine\c\src\video.c" ^
  "%ROOT%\engine\c\tests\test_main.c" ^
  "%ROOT%\engine\c\tests\test_memory.c" ^
  "%ROOT%\engine\c\tests\test_cpu.c" ^
  "%ROOT%\engine\c\tests\test_protocol.c" ^
  "%ROOT%\engine\c\tests\test_service.c" ^
  "%ROOT%\engine\c\tests\test_video.c" ^
  /Fe:yt2core-tests.exe
if errorlevel 1 goto fail

echo build: c -^> %BUILD%
popd
exit /b 0

:fail
popd
exit /b 1
