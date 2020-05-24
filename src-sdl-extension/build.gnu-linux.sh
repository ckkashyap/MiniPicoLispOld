#!/bin/sh

gcc ../src/gen3m.c -o gen3m.exe
./gen3m.exe 0 ../src/init.s ../src/lib.s ../src/pilog.s extra.s
gcc -falign-functions -I . -I ../src ../src/main.c ../src/gc.c ../src/apply.c ../src/flow.c ../src/sym.c ../src/subr.c ../src/math.c ../src/io.c extra_posix.c -ldl -o picolisp.exe
gcc `pkg-config --cflags sdl2` -fPIC -shared glue.c `pkg-config --libs sdl2` -o libglue.so 
