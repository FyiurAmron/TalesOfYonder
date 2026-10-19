ml /c swreg.asm
if errorlevel 1 goto end
link /SE:512 swreg;
:end
