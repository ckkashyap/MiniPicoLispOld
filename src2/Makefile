genfiles = gen.c error.c
lispfiles = lisp.c heapmem.c error.c

lisp: mem.d sym.d $(lispfiles:.c=.o) 
	gcc -o lisp $(lispfiles:.c=.o) 
 
gen: $(genfiles:.c=.o) 
	gcc -o gen $^
 
sym.d mem.d: mem.s gen
	./gen mem.s

lisp.o:lisp.c mem.d
	gcc -g3 -c $<

%.o:%.c
	gcc -g3 -c $<

clean:
	rm -f *.o gen lisp mem.d sym.d

