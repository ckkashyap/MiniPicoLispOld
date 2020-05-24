cl /Zi ..\src\gen3m.c
gen3m.exe 0 ..\src\init.s ..\src\lib.s ..\src\pilog.s extra.s
cl /Zi /Fe: picolisp.exe /I . /I ..\src ..\src\main.c ..\src\gc.c ..\src\apply.c ..\src\flow.c ..\src\sym.c ..\src\subr.c ..\src\math.c ..\src\io.c extra.c
cl /I SDL2-2.0.12\include SDL2-2.0.12\lib\x64\SDL2.lib /LD glue.c /Fe:glue.dll
copy /y SDL2-2.0.12\lib\x64\SDL2.dll
