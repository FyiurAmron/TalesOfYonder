uasm -nologo -mz -Fo=SWREG.EXE swreg.asm
if errorlevel 1 exit /b 1
python fixhdr.py SWREG.EXE
certutil -hashfile SWREG.EXE MD5
