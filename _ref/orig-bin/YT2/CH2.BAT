@ECHO OFF
REM Environment varible "SWGAMES" is preset to zero
REM Then if a "BLASTER" variable is found, "SWGAMES" is set to one
REM Then if a "SOUND" variable is found, "SWGAMES" is set to two
REM The variable "SWGAMES" is tested and the following is done:
REM   0 - runs game program without loading SBFMDRV
REM   1 - will set "SOUND" variable to "CHAPTER2"
REM       will load sbfmdrv.com from this directory
REM       will run the game program
REM       at exit will unload the SBFMDRV.COM tsr program
REM       at exit will clear "SOUND" variable
REM   2 - will load sbfmdrv.com from %SOUND%
REM       will run the game program
REM       at exit will unload the SBFMDRV.COM tsr program
REM If the game directory name is different than "CHAPTER2"
REM line 29 must be modified to reflect the correct directory name


SET SWGAMES=0

IF "" == "%BLASTER%" GOTO RUNP
SET SWGAMES=1
IF "" == "%SOUND%" GOTO NOSOUND
if not exist %sound%\sbfmdrv.com goto nosound
SET SWGAMES=2
%SOUND%\SBFMDRV > NUL
GOTO RUNP

:NOSOUND
SET SOUND=CHAPTER2
SBFMDRV > NUL

:RUNP
IF EXIST SWREG.EXE GOTO REGISTER
IF EXIST SWSHARE.EXE GOTO SHAREWARE
GOTO END

:SHAREWARE
SWSHARE.EXE %1 %2 %3 %4 /W /B
GOTO END

:REGISTER
SWREG.EXE %1 %2 %3 %4 /W /B

:END
IF %SWGAMES% == 0 GOTO EXIT
IF %SWGAMES% == 1 GOTO REL1

if not exist %sound%\sbfmdrv.com goto rel1
%SOUND%\SBFMDRV /U > NUL
GOTO EXIT

:REL1
SBFMDRV /U > NUL
SET SOUND=

:EXIT
SET SWGAMES=
