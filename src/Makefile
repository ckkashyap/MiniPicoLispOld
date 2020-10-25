# 12dec16abu
# (c) Software Lab. Alexander Burger

.SILENT:

bin = ../bin
picoFiles = main.c

lispFiles = lisp.c

CC = gcc

picolisp: $(bin)/picolisp lisp

.c.o:
	echo $*.c:
	$(CC) -c -g3 -pipe \
	-falign-functions -fomit-frame-pointer -fno-strict-aliasing \
	-W -Wimplicit -Wreturn-type -Wunused -Wformat \
	-Wuninitialized -Wstrict-prototypes \
	-D_GNU_SOURCE  $*.c

lisp: $(lispFiles:.c=.o)
	$(CC) -g3 -o lisp $(lispFiles:.c=.o) -lc -lm

$(bin)/picolisp: $(picoFiles:.c=.o)
	mkdir -p $(bin)
	echo "  " link picolisp:
	$(CC) -g3 -o $(bin)/picolisp $(picoFiles:.c=.o) -lc -lm

$(picoFiles:.c=.o): sym.d rom.d ram.d

sym.d rom.d ram.d: gen3m init.s
	./gen3m 0 init.s

gen3m: gen3m.c
	@echo "Building gen3m"
	$(CC) -o gen3m gen3m.c

$(lispFiles:.c=.o): mem.d def.d

mem.d: gen mem.s
	./gen mem.s

gen: gen.c
	@echo "Building gen"
	$(CC) -o gen gen.c
	
# Clean up
clean:
	rm -f gen3m *.d *.o

# vi:noet:ts=4:sw=4
