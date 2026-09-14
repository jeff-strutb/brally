@echo off
IF "%V5TOOLS%" == "" goto Usage1

setlocal
IF "%1" == "PMAC" goto buildpmac
IF "%1" == "PMac" goto buildpmac
IF "%1" == "pmac" goto buildpmac
IF "%1" == "" goto dobuild
goto Usage2

:buildpmac
shift
set PROCESSOR_ARCHITECTURE=PMAC
set PATH=%V5TOOLS%\mac\mppc\bin;%V5TOOLS%\mac\bin;%PATH%
set INCLUDE=%V5TOOLS%\mac\include;%V5TOOLS%\mac\include\macos;%INCLUDE%
set LIB=%V5TOOLS%\mac\mppc\lib;%LIB%

:dobuild
nmake %1 %2 %3 %4 %5 %6 %7 %8
endlocal

goto End

:Usage1
echo The environment variable V5TOOLS must be set to point
echo to the root of your VC++ 5.0 installation.

goto End

:Usage2
echo "bldnt" builds the runtimes for Win32 platform.
echo "bldnt PMac" builds the runtimes for the Power Macintosh.

:End

