@echo off

set PN=glyphe
set PP=C:\Dev\c\%PN%
set E=%PP%\e

set ccflags=/Od /utf-8 /D_CRT_SECURE_NO_WARNINGS /std:clatest -MTd -nologo -fp:fast -fp:except- -Gm- -GR- -EHa- -Zo -Oi -WX -W4 -wd4624 -wd4530 -wd4244 -wd4201 -wd4100 -wd4101 -wd4189 -wd4505 -wd4127 -wd4702 -wd4310 -FC -Z7 -I%PP%/q
set ldflags= -incremental:no -opt:ref

rmdir /Q /S %E%
mkdir %E%
pushd %E%

cls

cl %ccflags% %PP%\b\prog.c -Feprog.exe /link %ldflags%

popd
