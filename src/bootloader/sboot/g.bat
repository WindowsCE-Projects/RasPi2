@echo off
rem pushd ..
build %1
if EXIST build.err goto skip1
rem popd

del %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 
del %_flatreleasedir%\sldr.nb0

call romimage sboot.bib

patchfile %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 0 fe
patchfile %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 1 03
rem patchfile %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 2 00
patchfile %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 3 ea

copy %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.nb0 %_flatreleasedir%
copy %SG_OUTPUT_ROOT%\platform\%_TGTPLAT%\target\%_TGTCPU%\%WINCEDEBUG%\sldr.bin %_flatreleasedir%\nk.bin

copy %_flatreleasedir%\sldr.nb0 f:\kernel.img

goto end1
:skip1
rem popd
echo.
echo ****************
echo Errors in build.
echo ****************
echo.
:end1