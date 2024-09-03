@echo off

set CompilerFlags= -nologo -Od -Oi -WX -W4 -wd4100 -wd4200 -wd4201 -Fetest.exe -DDEBUG -Zi
set LinkerFlags= -fixed -incremental:no -opt:icf -opt:ref -subsystem:console libvcruntime.lib kernel32.lib

IF NOT EXIST bin mkdir bin
pushd bin
cl %CompilerFlags% ..\test.cpp /link %LinkerFlags%
popd

