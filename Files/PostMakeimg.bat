@echo off
echo Running %0
rem
rem Checks that IMGFLASH is set
rem
if .%IMGFLASH%. == .. goto NoFlash

rem
rem Check that nk not too big
rem
if NOT EXIST nk.nb1 goto sizegood
echo.
echo Error! NK.BIN too big for bib settings.  Change NKLEN in config.bib
echo.
goto endit

:sizegood
rem
rem This patches the binary version of the file with the proper jump address
rem
%_flatreleasedir%\patchfile %_flatreleasedir%\nk.nb0 0 fe
%_flatreleasedir%\patchfile %_flatreleasedir%\nk.nb0 1 03
%_flatreleasedir%\patchfile %_flatreleasedir%\nk.nb0 2 00
%_flatreleasedir%\patchfile %_flatreleasedir%\nk.nb0 3 ea

rem
rem Delete existing kernel.img if existing and rename nk.nb0 to kernel.img
rem this is much faster than a copy of nk.nb0
rem
pushd %_flatreleasedir%
if NOT EXIST kernel.img goto nokernyet
del kernel.img
:nokernyet
echo renaming nk.nb0 to kernel.img
rename nk.nb0  kernel.img
popd

goto endit
:NoFlash
echo. 
echo IMGFLASH not set.  It must be set to create kernel.img
echo. 
:endit