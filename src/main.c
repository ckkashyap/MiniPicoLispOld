#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>

#ifndef CELLS
#define CELLS (1024*1024/sizeof(cell))
#endif

#define WORD ((int)sizeof(long))
#define BITS (8*WORD)

typedef unsigned long word;
typedef unsigned char byte;
typedef unsigned char *ptr;

#undef bool
typedef enum {NO,YES} bool;

typedef struct cell {            // PicoLisp primary data type
   struct cell *car;
   struct cell *cdr;
} cell, *any;

typedef any (*fun)(any);

#include "sym.d"

typedef struct heap {
   cell cells[CELLS];
   struct heap *next;
} heap;

typedef struct bindFrame {
   struct bindFrame *link;
   int i, cnt;
   struct {any sym; any val;} bnd[1];
} bindFrame;

typedef struct inFrame {
   struct inFrame *link;
   void (*get)(void);
   FILE *fp;
   int next;
} inFrame;

typedef struct outFrame {
   struct outFrame *link;
   void (*put)(int);
   FILE *fp;
} outFrame;

typedef struct parseFrame {
   int i;
   word w;
   any sym, nm;
} parseFrame;

typedef struct stkEnv {
   cell *stack, *arg;
   bindFrame *bind;
   int next;
   any key, cls, *make, *yoke;
   inFrame *inFrames;
   outFrame *outFrames;
   parseFrame *parser;
   void (*get)(void);
   void (*put)(int);
   bool brk;
} stkEnv;

typedef struct catchFrame {
   struct catchFrame *link;
   any tag, fin;
   stkEnv env;
   jmp_buf rst;
} catchFrame;

/*** Macros ***/
#define Free(p)         ((p)->car=Avail, Avail=(p))

/* Number access */
#define num(x)          ((long)(x))
#define txt(n)          ((any)(num(n)<<1|1))
#define box(n)          ((any)(num(n)<<2|2))
#define unBox(n)        (num(n)>>2)
#define Zero            ((any)2)
#define One             ((any)6)

/* Symbol access */
#define symPtr(x)       ((any)&(x)->cdr)
#define val(x)          ((x)->car)
#define tail(x)         (((x)-1)->cdr)

/* Cell access */
#define car(x)          ((x)->car)
#define cdr(x)          ((x)->cdr)
#define caar(x)         (car(car(x)))
#define cadr(x)         (car(cdr(x)))
#define cdar(x)         (cdr(car(x)))
#define cddr(x)         (cdr(cdr(x)))
#define caaar(x)        (car(car(car(x))))
#define caadr(x)        (car(car(cdr(x))))
#define cadar(x)        (car(cdr(car(x))))
#define caddr(x)        (car(cdr(cdr(x))))
#define cdaar(x)        (cdr(car(car(x))))
#define cdadr(x)        (cdr(car(cdr(x))))
#define cddar(x)        (cdr(cdr(car(x))))
#define cdddr(x)        (cdr(cdr(cdr(x))))
#define cadddr(x)       (car(cdr(cdr(cdr(x)))))
#define cddddr(x)       (cdr(cdr(cdr(cdr(x)))))

#define data(c)         ((c).car)
#define Save(c)         ((c).cdr=Env.stack, Env.stack=&(c))
#define drop(c)         (Env.stack=(c).cdr)
#define Push(c,x)       (data(c)=(x), Save(c))
#define Pop(c)          (drop(c), data(c))

#define Bind(s,f)       ((f).i=0, (f).cnt=1, (f).bnd[0].sym=(s), (f).bnd[0].val=val(s), (f).link=Env.bind, Env.bind=&(f))
#define Unbind(f)       (val((f).bnd[0].sym)=(f).bnd[0].val, Env.bind=(f).link)

/* Predicates */
#define isNil(x)        ((x)==Nil)
#define isTxt(x)        (num(x)&1)
#define isNum(x)        (num(x)&2)
#define isSym(x)        (num(x)&WORD)
#define isSymb(x)       ((num(x)&(WORD+2))==WORD)
#define isCell(x)       (!(num(x)&(2*WORD-1)))

/* Evaluation */
#define EVAL(x)         (isNum(x)? x : isSym(x)? val(x) : evList(x))
#define evSubr(f,x)     (*(fun)(num(f) & ~2))(x)

/* Error checking */
#define NeedNum(ex,x)   if (!isNum(x)) numError(ex,x)
#define NeedSym(ex,x)   if (!isSym(x)) symError(ex,x)
#define NeedSymb(ex,x)  if (!isSymb(x)) symError(ex,x)
#define NeedPair(ex,x)  if (!isCell(x)) pairError(ex,x)
#define NeedAtom(ex,x)  if (isCell(x)) atomError(ex,x)
#define NeedLst(ex,x)   if (!isCell(x) && !isNil(x)) lstError(ex,x)
#define NeedVar(ex,x)   if (isNum(x)) varError(ex,x)
#define CheckVar(ex,x)  if ((x)>(any)Rom && (x)<=Quote) protError(ex,x)

/* Globals */
extern int Chr, Trace;
extern char **AV, *AV0, *Home;
extern heap *Heaps;
extern cell *Avail;
extern stkEnv Env;
extern catchFrame *CatchPtr;
extern FILE *InFile, *OutFile;
extern any TheKey, TheCls, Thrown;
extern any Intern[2], Transient[2];
extern any ApplyArgs, ApplyBody;
extern any const Rom[];
extern any Ram[];

/* Prototypes */
void *alloc(void*,size_t);
any apply(any,any,bool,int,cell*);
void argError(any,any) __attribute__ ((noreturn));
void atomError(any,any) __attribute__ ((noreturn));
void begString(void);
void brkLoad(any);
int bufNum(char[BITS/2],long);
int bufSize(any);
void bufString(any,char*);
void bye(int) __attribute__ ((noreturn));
void pairError(any,any) __attribute__ ((noreturn));
any circ(any);
long compare(any,any);
any cons(any,any);
any consName(word,any);
any consSym(any,word);
void newline(void);
any endString(void);
bool equal(any,any);
void err(any,any,char*,...) __attribute__ ((noreturn));
any evExpr(any,any);
any evList(any);
long evNum(any,any);
any evSym(any);
void execError(char*) __attribute__ ((noreturn));
int firstByte(any);
any get(any,any);
int getByte(int*,word*,any*);
int getByte1(int*,word*,any*);
void getStdin(void);
void giveup(char*) __attribute__ ((noreturn));
void heapAlloc(void);
any intern(any,any[2]);
any isIntern(any,any[2]);
void lstError(any,any) __attribute__ ((noreturn));
any load(any,int,any);
any loadAll(any);
any method(any);
any mkChar(int);
any mkSym(byte*);
any mkStr(char*);
any mkTxt(int);
any name(any);
int numBytes(any);
void numError(any,any) __attribute__ ((noreturn));
any numToSym(any,int,int,int);
void outName(any);
void outNum(long);
void outString(char*);
void pack(any,int*,word*,any*,cell*);
int pathSize(any);
void pathString(any,char*);
void popInFiles(void);
void popOutFiles(void);
any popSym(int,word,any,cell*);
void prin(any);
void print(any);
void protError(any,any) __attribute__ ((noreturn));
void pushInFiles(inFrame*);
void pushOutFiles(outFrame*);
void put(any,any,any);
void putByte(int,int*,word*,any*,cell*);
void putByte1(int,int*,word*,any*);
void putStdout(int);
void rdOpen(any,any,inFrame*);
any read1(int);
void space(void);
int symBytes(any);
void symError(any,any) __attribute__ ((noreturn));
any symToNum(any,int,int,int);
void undefined(any,any);
void unintern(any,any[2]);
void unwind (catchFrame*);
void varError(any,any) __attribute__ ((noreturn));
void wrOpen(any,any,outFrame*);
long xNum(any,any);
any xSym(any);

/* List length calculation */
static inline int length(any x) {
   int n;

   for (n = 0; isCell(x); x = cdr(x))
      ++n;
   return n;
}

/* List interpreter */
static inline any prog(any x) {
   any y;

   do
      y = EVAL(car(x));
   while (isCell(x = cdr(x)));
   return y;
}

static inline any run(any x) {
   any y;
   cell at;

   Push(at,val(At));
   do
      y = EVAL(car(x));
   while (isCell(x = cdr(x)));
   val(At) = Pop(at);
   return y;
}
/* Globals */
int Chr, Trace;
char **AV, *AV0, *Home;
heap *Heaps;
cell *Avail;
stkEnv Env;
catchFrame *CatchPtr;
FILE *InFile, *OutFile;
any Intern[2], Transient[2];
any ApplyArgs, ApplyBody;

/* ROM Data */
any const __attribute__ ((__aligned__(2*WORD))) Rom[] = {
   #include "rom.d"
};

/* RAM Symbols */
any __attribute__ ((__aligned__(2*WORD))) Ram[] = {
   #include "ram.d"
};

static jmp_buf ErrRst;

///////////////////////////////////////////////
//               sym.c
///////////////////////////////////////////////

int firstByte(any s) {
   int c;

   if (isNil(s))
      return 0;
   c = (word)s;
   return c & 127;
}

int getByte1(int *i, word *p, any *q) {
   int c;

   *i = BITS - 1, *p = (word)*q >> 1, *q = NULL;

   c = *p & 127, *p >>= 8, *i -= 8;

   return c;
}

int getByte(int *i, word *p, any *q) {
   int c;

   if (*i == 0) {
      if (!*q)
         return 0;
      if (isNum(*q))
         *i = BITS-2,  *p = (word)*q >> 2,  *q = NULL;
      else
         *i = BITS,  *p = (word)tail(*q),  *q = val(*q);
   }
   if (1) {
      c = *p & 127,  *p >>= 8;
      if (*i >= 8)
         *i -= 8;
      else if (isNum(*q)) {
         *p = (word)*q >> 2,  *q = NULL;
         c |= *p << *i;
         *p >>= 8 - *i;
         *i += BITS-9;
      }
      else {
         *p = (word)tail(*q),  *q = val(*q);
         c |= *p << *i;
         *p >>= 8 - *i;
         *i += BITS-8;
      }
      c &= 127;
   }
   return c;
}

any mkTxt(int c) {return txt(c & 127);}

any mkChar(int c) {
   return consSym(NULL, c & 127);
}

void putByte1(int c, int *i, word *p, any *q) {
   *p = c & 127;
   *i = 8;
   *q = NULL;
}

void putByte(int c, int *i, word *p, any *q, cell *cp) {
   c = c & 127;
   int d = 8;

   if (*i != BITS)
      *p |= (word)c << *i;
   if (*i + d  > BITS) {
      if (*q)
         *q = val(*q) = consName(*p, Zero);
      else {
         Push(*cp, consSym(NULL,0));
         tail(data(*cp)) = *q = consName(*p, Zero);
      }
      *p = c >> BITS - *i;
      *i -= BITS;
   }
   *i += d;
}

any popSym(int i, word n, any q, cell *cp) {
   if (q) {
      val(q) = i <= (BITS-2)? box(n) : consName(n, Zero);
      return Pop(*cp);
   }
   if (i > BITS-1) {
      Push(*cp, consSym(NULL,0));
      tail(data(*cp)) = consName(n, Zero);
      return Pop(*cp);
   }
   return consSym(NULL,n);
}

int symBytes(any x) {
   int cnt = 0;
   word w;

   if (isNil(x))
      return 0;
   x = name(x);
   if (isTxt(x)) {
      w = (word)x >> 1;
      while (w)
         ++cnt,  w >>= w & 1? 7 : 6;
   }
   else {
      do {
         w = (word)tail(x);
         do
            ++cnt;
         while (w >>= w & 1? 7 : 6);
      } while (!isNum(x = val(x)));
      w = (word)x >> 2;
      while (w)
         ++cnt,  w >>= w & 1? 7 : 6;
   }
   return cnt;
}

any isIntern(any nm, any tree[2]) {
   any x, y, z;
   long n;

   if (isTxt(nm)) {
      for (x = tree[0];  isCell(x);) {
         if ((n = (word)nm - (word)name(car(x))) == 0)
            return car(x);
         x = n<0? cadr(x) : cddr(x);
      }
   }
   else {
      for (x = tree[1];  isCell(x);) {
         y = nm,  z = name(car(x));
         for (;;) {
            if ((n = (word)tail(y) - (word)tail(z)) != 0) {
               x = n<0? cadr(x) : cddr(x);
               break;
            }
            y = val(y),  z = val(z);
            if (isNum(y)) {
               if (y == z)
                  return car(x);
               x = isNum(z) && y>z? cddr(x) : cadr(x);
               break;
            }
            if (isNum(z)) {
               x = cddr(x);
               break;
            }
         }
      }
   }
   return NULL;
}

any intern(any sym, any tree[2])
{
   any nm, x, y, z;
   long n;

   if ((nm = name(sym)) == txt(0))
      return sym;

   if (!isCell(x = tree[0]))
   {
      tree[0] = cons(sym, Nil);
      return sym;
   }
   for (;;)
   {
      if ((n = (word)nm - (word)name(car(x))) == 0)
         return car(x);
      if (!isCell(cdr(x)))
      {
         cdr(x) = n < 0 ? cons(cons(sym, Nil), Nil) : cons(Nil, cons(sym, Nil));
         return sym;
      }
      if (n < 0)
      {
         if (isCell(cadr(x)))
            x = cadr(x);
         else
         {
            cadr(x) = cons(sym, Nil);
            return sym;
         }
      }
      else
      {
         if (isCell(cddr(x)))
            x = cddr(x);
         else
         {
            cddr(x) = cons(sym, Nil);
            return sym;
         }
      }
   }
}

void unintern(any sym, any tree[2]) {
   any nm, x, y, z, *p;
   long n;

   if ((nm = name(sym)) == txt(0))
      return;
   if (isTxt(nm)) {
      if (!isCell(x = tree[0]))
         return;
      p = &tree[0];
      for (;;) {
         if ((n = (word)nm - (word)name(car(x))) == 0) {
            if (car(x) == sym) {
               if (!isCell(cadr(x)))
                  *p = cddr(x);
               else if (!isCell(y = cddr(x)))
                  *p = cadr(x);
               else if (!isCell(z = cadr(y)))
                  car(x) = car(y),  cddr(x) = cddr(y);
               else {
                  while (isCell(cadr(z)))
                     z = cadr(y = z);
                  car(x) = car(z),  cadr(y) = cddr(z);
               }
            }
            return;
         }
         if (!isCell(cdr(x)))
            return;
         if (n < 0) {
            if (!isCell(cadr(x)))
               return;
            x = *(p = &cadr(x));
         }
         else {
            if (!isCell(cddr(x)))
               return;
            x = *(p = &cddr(x));
         }
      }
   }
   else {
      if (!isCell(x = tree[1]))
         return;
      p = &tree[1];
      for (;;) {
         y = nm,  z = name(car(x));
         while ((n = (word)tail(y) - (word)tail(z)) == 0) {
            y = val(y),  z = val(z);
            if (isNum(y)) {
               if (y == z) {
                  if (car(x) == sym) {
                     if (!isCell(cadr(x)))
                        *p = cddr(x);
                     else if (!isCell(y = cddr(x)))
                        *p = cadr(x);
                     else if (!isCell(z = cadr(y)))
                        car(x) = car(y),  cddr(x) = cddr(y);
                     else {
                        while (isCell(cadr(z)))
                           z = cadr(y = z);
                        car(x) = car(z),  cadr(y) = cddr(z);
                     }
                  }
                  return;
               }
               n = isNum(z)? y-z : -1;
               break;
            }
            if (isNum(z)) {
               n = +1;
               break;
            }
         }
         if (!isCell(cdr(x)))
            return;
         if (n < 0) {
            if (!isCell(cadr(x)))
               return;
            x = *(p = &cadr(x));
         }
         else {
            if (!isCell(cddr(x)))
               return;
            x = *(p = &cddr(x));
         }
      }
   }
}

/* Get symbol name */
any name(any s) {
   for (s = tail(s); isCell(s); s = car(s));
   return s;
}

/* Make name */
any mkSym(byte *s) {
   int i;
   word w;
   cell c1, *p;

   putByte1(*s++, &i, &w, &p);
   while (*s)
      putByte(*s++, &i, &w, &p, &c1);
   return popSym(i, w, p, &c1);
}

/* Make string */
any mkStr(char *s) {return s && *s? mkSym((byte*)s) : Nil;}


// (==== ['sym ..]) -> NIL
any doHide(any ex) {
   any x, y;

   Transient[0] = Transient[1] = Nil;
   for (x = cdr(ex); isCell(x); x = cdr(x)) {
      y = EVAL(car(x));
      NeedSymb(ex,y);
      intern(y, Transient);
   }
   return Nil;
}


any doCons(any x) {
   any y;
   cell c1;

   x = cdr(x);
   Push(c1, y = cons(EVAL(car(x)),Nil));
   while (isCell(cdr(x = cdr(x))))
      y = cdr(y) = cons(EVAL(car(x)),Nil);
   cdr(y) = EVAL(car(x));
   return Pop(c1);
}


// (setq var 'any ..) -> any
any doSetq(any ex) {
   any x, y;

   x = cdr(ex);
   do {
      y = car(x),  x = cdr(x);
      NeedVar(ex,y);
      CheckVar(ex,y);
      val(y) = EVAL(car(x));
   } while (isCell(x = cdr(x)));
   return val(y);
}
static void makeError(any ex) {err(ex, NULL, "Not making");}

// (link 'any ..) -> any
any doLink(any x) {
   any y;

   if (!Env.make)
      makeError(x);
   x = cdr(x);
   do {
      y = EVAL(car(x));
      Env.make = &cdr(*Env.make = cons(y, Nil));
   } while (isCell(x = cdr(x)));
   return y;
}

// (make .. [(made 'lst ..)] .. [(link 'any ..)] ..) -> any
any doMake(any x) {
   any *make, *yoke;
   cell c1;

   Push(c1, Nil);
   make = Env.make;
   yoke = Env.yoke;
   Env.make = Env.yoke = &data(c1);
   while (isCell(x = cdr(x)))
      if (isCell(car(x)))
         evList(car(x));
   Env.yoke = yoke;
   Env.make = make;
   return Pop(c1);
}

///////////////////////////////////////////////
//               sym.c - END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               io.c - START
///////////////////////////////////////////////

static any read0(bool);

static char Delim[] = " \t\n\r\"'(),[]`~{}";

static void openErr(any ex, char *s) {err(ex, NULL, "%s open: %s", s, strerror(errno));}
static void eofErr(void) {err(NULL, NULL, "EOF Overrun");}

/* Buffer size */
int bufSize(any x) {return symBytes(x) + 1;}

int pathSize(any x) {
   int c = firstByte(x);

   if (c != '@'  &&  (c != '+'))
      return bufSize(x);
   if (!Home)
      return symBytes(x);
   return strlen(Home) + symBytes(x);
}

void bufString(any x, char *p) {
   int c, i;
   word w;

   if (!isNil(x)) {
      for (x = name(x), c = getByte1(&i, &w, &x); c; c = getByte(&i, &w, &x)) {
         if (c == '^') {
            if ((c = getByte(&i, &w, &x)) == '?')
               c = 127;
            else
               c &= 0x1F;
         }
         *p++ = c;
      }
   }
   *p = '\0';
}

void pathString(any x, char *p) {
   int c, i;
   word w;
   char *h;

   x = name(x);
   if ((c = getByte1(&i, &w, &x)) == '+')
      *p++ = c,  c = getByte(&i, &w, &x);
   if (c != '@')
      while (*p++ = c)
         c = getByte(&i, &w, &x);
   else {
      if (h = Home)
         do
            *p++ = *h++;
         while (*h);
      while (*p++ = getByte(&i, &w, &x));
   }
}

void rdOpen(any ex, any x, inFrame *f) {
   NeedSymb(ex,x);
   if (isNil(x))
      f->fp = stdin;
   else {
      char nm[pathSize(x)];

      pathString(x,nm);
      if (nm[0] == '+') {
         if (!(f->fp = fopen(nm+1, "a+")))
            openErr(ex, nm);
         fseek(f->fp, 0L, SEEK_SET);
      }
      else if (!(f->fp = fopen(nm, "r")))
         openErr(ex, nm);
   }
}

/*** Reading ***/
void getStdin(void) {Chr = getc(InFile);}

static void getParse(void) {
   if ((Chr = getByte(&Env.parser->i, &Env.parser->w, &Env.parser->nm)) == 0)
      Chr = ']';
}

void pushInFiles(inFrame *f) {
   f->next = Chr,  Chr = 0;
   InFile = f->fp;
   f->get = Env.get,  Env.get = getStdin;
   f->link = Env.inFrames,  Env.inFrames = f;
}

void pushOutFiles(outFrame *f) {
   OutFile = f->fp;
   f->put = Env.put,  Env.put = putStdout;
   f->link = Env.outFrames,  Env.outFrames = f;
}

void popInFiles(void) {
   if (InFile != stdin)
      fclose(InFile);
   Chr = Env.inFrames->next;
   Env.get = Env.inFrames->get;
   InFile = (Env.inFrames = Env.inFrames->link)?  Env.inFrames->fp : stdin;
}

void popOutFiles(void) {
   if (OutFile != stdout && OutFile != stderr)
      fclose(OutFile);
   Env.put = Env.outFrames->put;
   OutFile = (Env.outFrames = Env.outFrames->link)? Env.outFrames->fp : stdout;
}

/* Skip White Space and Comments */
static int skipc(int c) {
   if (Chr < 0)
      return Chr;
   for (;;) {
      while (Chr <= ' ') {
         Env.get();
         if (Chr < 0)
            return Chr;
      }
      if (Chr != c)
         return Chr;
      Env.get();
      while (Chr != '\n') {
         if (Chr < 0)
            return Chr;
         Env.get();
      }
   }
}

static void comment(void) {
   Env.get();
   if (Chr != '{') {
      while (Chr != '\n') {
         if (Chr < 0)
            return;
         Env.get();
      }
   }
   else {
      int n = 0;

      for (;;) {  // #{block-comment}# from Kriangkrai Soatthiyanont
         Env.get();
         if (Chr < 0)
            return;
         if (Chr == '#'  &&  (Env.get(), Chr == '{'))
            ++n;
         else if (Chr == '}'  &&  (Env.get(), Chr == '#')  &&  --n < 0)
            break;
      }
      Env.get();
   }
}

static int skip(void) {
   for (;;) {
      if (Chr < 0)
         return Chr;
      while (Chr <= ' ') {
         Env.get();
         if (Chr < 0)
            return Chr;
      }
      if (Chr != '#')
         return Chr;
      comment();
   }
}

/* Test for escaped characters */
static bool testEsc(void) {
   for (;;) {
      if (Chr < 0)
         return NO;
      if (Chr != '\\')
         return YES;
      if (Env.get(), Chr != '\n')
         return YES;
      do
         Env.get();
      while (Chr == ' '  ||  Chr == '\t');
   }
}

/* Read a list */
static any rdList(void) {
   any x;
   cell c1;

   for (;;) {
      if (skip() == ')') {
         Env.get();
         return Nil;
      }
      if (Chr == ']')
         return Nil;
      if (Chr != '~') {
         Push(c1, x = cons(read0(NO),Nil));
         break;
      }
      Env.get();
      Push(c1, read0(NO));
      if (isCell(x = data(c1) = EVAL(data(c1)))) {
         while (isCell(cdr(x)))
            x = cdr(x);
         break;
      }
      drop(c1);
   }
   for (;;) {
      if (skip() == ')') {
         Env.get();
         break;
      }
      if (Chr == ']')
         break;
      if (Chr == '.') {
         Env.get();
         cdr(x) = skip()==')' || Chr==']'? data(c1) : read0(NO);
         if (skip() == ')')
            Env.get();
         else if (Chr != ']')
            err(NULL, x, "Bad dotted pair");
         break;
      }
      if (Chr != '~')
         x = cdr(x) = cons(read0(NO),Nil);
      else {
         Env.get();
         cdr(x) = read0(NO);
         cdr(x) = EVAL(cdr(x));
         while (isCell(cdr(x)))
            x = cdr(x);
      }
   }
   return Pop(c1);
}

/* Try for anonymous symbol */
static any anonymous(any s) {
   int c, i;
   word w;
   unsigned long n;
   heap *h;

   if ((c = getByte1(&i, &w, &s)) != '$')
      return NULL;
   n = 0;
   while (c = getByte(&i, &w, &s)) {
      if (c < '0' || c > '9')
         return NULL;
      n = n * 10 + c - '0';
   }
   n *= sizeof(cell);
   h = Heaps;
   do
      if ((any)n > h->cells  &&  (any)n < h->cells + CELLS)
         return symPtr((any)n);
   while (h = h->next);
   return NULL;
}

/* Read one expression */
static any read0(bool top) {
   int i;
   word w;
   any x, y;
   cell c1, *p;

   if (skip() < 0) {
      if (top)
         return Nil;
      eofErr();
   }
   if (Chr == '(') {
      Env.get();
      x = rdList();
      if (top  &&  Chr == ']')
         Env.get();
      return x;
   }
   if (Chr == '[') {
      Env.get();
      x = rdList();
      if (Chr != ']')
         err(NULL, x, "Super parentheses mismatch");
      Env.get();
      return x;
   }
   if (Chr == '\'') {
      Env.get();
      return cons(Quote, read0(top));
   }
   if (Chr == ',') {
      Env.get();
      return read0(top);
   }
   if (Chr == '`') {
      Env.get();
      Push(c1, read0(top));
      x = EVAL(data(c1));
      drop(c1);
      return x;
   }
   if (Chr == '"') {
      Env.get();
      if (Chr == '"') {
         Env.get();
         return Nil;
      }
      if (!testEsc())
         eofErr();
      putByte1(Chr, &i, &w, &p);
      while (Env.get(), Chr != '"') {
         if (!testEsc())
            eofErr();
         putByte(Chr, &i, &w, &p, &c1);
      }
      y = popSym(i, w, p, &c1),  Env.get();
      if (x = isIntern(tail(y), Transient))
         return x;
      intern(y, Transient);
      return y;
   }
   if (strchr(Delim, Chr))
      err(NULL, NULL, "Bad input '%c' (%d)", isprint(Chr)? Chr:'?', Chr);
   if (Chr == '\\')
      Env.get();
   putByte1(Chr, &i, &w, &p);
   for (;;) {
      Env.get();
      if (strchr(Delim, Chr))
         break;
      if (Chr == '\\')
         Env.get();
      putByte(Chr, &i, &w, &p, &c1);
   }
   y = popSym(i, w, p, &c1);
   if (x = symToNum(tail(y), (int)unBox(val(Scl)), '.', 0))
      return x;
   if (x = anonymous(name(y)))
      return x;
   if (x = isIntern(tail(y), Intern))
      return x;
   intern(y, Intern);
   val(y) = Nil;
   return y;
}

any read1(int end) {
   if (!Chr)
      Env.get();
   if (Chr == end)
      return Nil;
   return read0(YES);
}

/* Read one token */
any token(any x, int c) {
   int i;
   word w;
   any y;
   cell c1, *p;

   if (!Chr)
      Env.get();
   if (skipc(c) < 0)
      return Nil;
   if (Chr == '"') {
      Env.get();
      if (Chr == '"') {
         Env.get();
         return Nil;
      }
      if (!testEsc())
         return Nil;
      Push(c1, y =  cons(mkChar(Chr), Nil));
      while (Env.get(), Chr != '"' && testEsc())
         y = cdr(y) = cons(mkChar(Chr), Nil);
      Env.get();
      return Pop(c1);
   }
   if (Chr >= '0' && Chr <= '9') {
      putByte1(Chr, &i, &w, &p);
      while (Env.get(), Chr >= '0' && Chr <= '9' || Chr == '.')
         putByte(Chr, &i, &w, &p, &c1);
      return symToNum(tail(popSym(i, w, p, &c1)), (int)unBox(val(Scl)), '.', 0);
   }
   if (Chr != '+' && Chr != '-') {
      char nm[bufSize(x)];

      bufString(x, nm);
      if (Chr >= 'A' && Chr <= 'Z' || Chr == '\\' || Chr >= 'a' && Chr <= 'z' || strchr(nm,Chr)) {
         if (Chr == '\\')
            Env.get();
         putByte1(Chr, &i, &w, &p);
         while (Env.get(),
               Chr >= '0' && Chr <= '9' || Chr >= 'A' && Chr <= 'Z' ||
               Chr == '\\' || Chr >= 'a' && Chr <= 'z' || strchr(nm,Chr) ) {
            if (Chr == '\\')
               Env.get();
            putByte(Chr, &i, &w, &p, &c1);
         }
         y = popSym(i, w, p, &c1);
         if (x = isIntern(tail(y), Intern))
            return x;
         intern(y, Intern);
         val(y) = Nil;
         return y;
      }
   }
   y = mkTxt(c = Chr);
   Env.get();
   return mkChar(c);
}


static inline bool eol(void) {
   if (Chr < 0)
      return YES;
   if (Chr == '\n') {
      Chr = 0;
      return YES;
   }
   if (Chr == '\r') {
      Env.get();
      if (Chr == '\n')
         Chr = 0;
      return YES;
   }
   return NO;
}

static any parse(any x, bool skp) {
   int c;
   parseFrame *save, parser;
   void (*getSave)(void);
   cell c1;

   save = Env.parser;
   Env.parser = &parser;
   parser.nm = name(parser.sym = x);
   getSave = Env.get,  Env.get = getParse,  c = Chr;
   Push(c1, Env.parser->sym);
   Chr = getByte1(&parser.i, &parser.w, &parser.nm);
   if (skp)
      getParse();
   x = rdList();
   drop(c1);
   Chr = c,  Env.get = getSave,  Env.parser = save;
   return x;
}

any load(any ex, int pr, any x) {
   cell c1, c2;
   inFrame f;

   if (isSymb(x) && firstByte(x) == '-') {
      Push(c1, parse(x,YES));
      x = evList(data(c1));
      drop(c1);
      return x;
   }
   rdOpen(ex, x, &f);
   pushInFiles(&f);
   doHide(Nil);
   x = Nil;
   for (;;) {
      if (InFile != stdin)
         data(c1) = read1(0);
      else {
         if (pr && !Chr)
            Env.put(pr), space(), fflush(OutFile);
         data(c1) = read1('\n');
         while (Chr > 0) {
            if (Chr == '\n') {
               Chr = 0;
               break;
            }
            if (Chr == '#')
               comment();
            else {
               if (Chr > ' ')
                  break;
               Env.get();
            }
         }
      }
      if (isNil(data(c1))) {
         popInFiles();
         doHide(Nil);
         return x;
      }
      Save(c1);
      if (InFile != stdin || Chr || !pr)
         x = EVAL(data(c1));
      else {
         Push(c2, val(At));
         x = val(At) = EVAL(data(c1));
         val(At3) = val(At2),  val(At2) = data(c2);
         outString("-> "),  fflush(OutFile),  print(x),  newline();
      }
      drop(c1);
   }
}

/*** Prining ***/
void putStdout(int c) {putc(c, OutFile);}

void newline(void) {Env.put('\n');}
void space(void) {Env.put(' ');}

void outString(char *s) {
   while (*s)
      Env.put(*s++);
}

int bufNum(char buf[BITS/2], long n) {
   return sprintf(buf, "%ld", n);
}

void outNum(long n) {
   char buf[BITS/2];

   bufNum(buf, n);
   outString(buf);
}

void prIntern(any nm) {
   int i, c;
   word w;

   c = getByte1(&i, &w, &nm);
   if (strchr(Delim, c))
      Env.put('\\');
   Env.put(c);
   while (c = getByte(&i, &w, &nm)) {
      if (c == '\\' || strchr(Delim, c))
         Env.put('\\');
      Env.put(c);
   }
}

void prTransient(any nm) {
   int i, c;
   word w;

   Env.put('"');
   c = getByte1(&i, &w, &nm);
   do {
      if (c == '"'  ||  c == '\\')
         Env.put('\\');
      Env.put(c);
   } while (c = getByte(&i, &w, &nm));
   Env.put('"');
}

/* Print one expression */
void print(any x) {
   if (isNum(x))
      outNum(unBox(x));
   else if (isSym(x)) {
      any nm = name(x);

      if (nm == txt(0))
         Env.put('$'),  outNum((word)x/sizeof(cell));
      else if (x == isIntern(nm, Intern))
         prIntern(nm);
      else
         prTransient(nm);
   }
   else if (car(x) == Quote  &&  x != cdr(x))
      Env.put('\''),  print(cdr(x));
   else {
      any y;

      Env.put('(');
      if ((y = circ(x)) == NULL) {
         while (print(car(x)), !isNil(x = cdr(x))) {
            if (!isCell(x)) {
               outString(" . ");
               print(x);
               break;
            }
            space();
         }
      }
      else if (y == x) {
         do
            print(car(x)),  space();
         while (y != (x = cdr(x)));
         Env.put('.');
      }
      else {
         do
            print(car(x)),  space();
         while (y != (x = cdr(x)));
         outString(". (");
         do
            print(car(x)),  space();
         while (y != (x = cdr(x)));
         outString(".)");
      }
      Env.put(')');
   }
}

void prin(any x) {
   if (!isNil(x)) {
      if (isNum(x))
         outNum(unBox(x));
      else if (isSym(x)) {
         int i, c;
         word w;

         for (x = name(x), c = getByte1(&i, &w, &x); c; c = getByte(&i, &w, &x)) {
            if (c != '^')
               Env.put(c);
            else if (!(c = getByte(&i, &w, &x)))
               Env.put('^');
            else if (c == '?')
               Env.put(127);
            else
               Env.put(c &= 0x1F);
         }
      }
      else {
         while (prin(car(x)), !isNil(x = cdr(x))) {
            if (!isCell(x)) {
               prin(x);
               break;
            }
         }
      }
   }
}

// (prin 'any ..) -> any
any doPrin(any x) {
   any y = Nil;

   while (isCell(x = cdr(x)))
      prin(y = EVAL(car(x)));
   newline();
   return y;
}

///////////////////////////////////////////////
//               io.c - END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               math.c START
///////////////////////////////////////////////

/* Number of bytes */
int numBytes(any x) {
   int n = 4;
   word w = (word)x >> 2;

   if ((w & 0xFF000000) == 0) {
      --n;
      if ((w & 0xFF0000) == 0) {
         --n;
         if ((w & 0xFF00) == 0)
            --n;
      }
   }
   return n;
}

/* Make number from symbol */
any symToNum(any s, int scl, int sep, int ign) {
   unsigned c;
   int i;
   word w;
   bool sign, frac;
   long n;

   if (!(c = getByte1(&i, &w, &s)))
      return NULL;
   while (c <= ' ')  /* Skip white space */
      if (!(c = getByte(&i, &w, &s)))
         return NULL;
   sign = NO;
   if (c == '+'  ||  c == '-' && (sign = YES))
      if (!(c = getByte(&i, &w, &s)))
         return NULL;
   if ((c -= '0') > 9)
      return NULL;
   frac = NO;
   n = c;
   while ((c = getByte(&i, &w, &s))  &&  (!frac || scl)) {
      if ((int)c == sep) {
         if (frac)
            return NULL;
         frac = YES;
      }
      else if ((int)c != ign) {
         if ((c -= '0') > 9)
            return NULL;
         n = n * 10 + c;
         if (frac)
            --scl;
      }
   }
   if (c) {
      if ((c -= '0') > 9)
         return NULL;
      if (c >= 5)
         n += 1;
      while (c = getByte(&i, &w, &s)) {
         if ((c -= '0') > 9)
            return NULL;
      }
   }
   if (frac)
      while (--scl >= 0)
         n *= 10;
   return box(sign? -n : n);
}

// (+ 'num ..) -> num
any doAdd(any ex) {
   any x, y;
   long n;

   x = cdr(ex);
   if (isNil(y = EVAL(car(x))))
      return Nil;
   NeedNum(ex,y);
   n = unBox(y);
   while (isCell(x = cdr(x))) {
      if (isNil(y = EVAL(car(x))))
         return Nil;
      NeedNum(ex,y);
      n += unBox(y);
   }
   return box(n);
}



///////////////////////////////////////////////
//               math.c END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               flow.c START
///////////////////////////////////////////////

static void redefMsg(any x, any y) {
   FILE *oSave = OutFile;

   OutFile = stderr;
   outString("# ");
   print(x);
   if (y)
      space(), print(y);
   outString(" redefined\n");
   OutFile = oSave;
}

static void redefine(any ex, any s, any x) {
   NeedSymb(ex,s);
   CheckVar(ex,s);
   if (!isNil(val(s))  &&  s != val(s)  &&  !equal(x,val(s)))
      redefMsg(s,NULL);
   val(s) = x;
}

// (quote . any) -> any
any doQuote(any x) {return cdr(x);}

// (de sym . any) -> sym
any doDe(any ex) {
   redefine(ex, cadr(ex), cddr(ex));
   return cadr(ex);
}

// (meth 'obj ..) -> any
any doMeth(any ex) {
    return ex;
}


// (let sym 'any . prg) -> any
// (let (sym 'any ..) . prg) -> any
any doLet(any x) {
   any y;

   x = cdr(x);
   if (!isCell(y = car(x))) {
      bindFrame f;

      x = cdr(x),  Bind(y,f),  val(y) = EVAL(car(x));
      x = prog(cdr(x));
      Unbind(f);
   }
   else {
      struct {  // bindFrame
         struct bindFrame *link;
         int i, cnt;
         struct {any sym; any val;} bnd[(length(y)+1)/2];
      } f;

      f.link = Env.bind,  Env.bind = (bindFrame*)&f;
      f.i = f.cnt = 0;
      do {
         f.bnd[f.cnt].sym = car(y);
         f.bnd[f.cnt].val = val(car(y));
         ++f.cnt;
         val(car(y)) = EVAL(cadr(y));
      } while (isCell(y = cddr(y)));
      x = prog(cdr(x));
      while (--f.cnt >= 0)
         val(f.bnd[f.cnt].sym) = f.bnd[f.cnt].val;
      Env.bind = f.link;
   }
   return x;
}

// (bye 'num|NIL)
any doBye(any ex) {
   printf("\n");
   bye(0);
   return ex;
}


// (for sym 'num ['any | (NIL 'any . prg) | (T 'any . prg) ..]) -> any
// (for sym|(sym2 . sym) 'lst ['any | (NIL 'any . prg) | (T 'any . prg) ..]) -> any
// (for (sym|(sym2 . sym) 'any1 'any2 [. prg]) ['any | (NIL 'any . prg) | (T 'any . prg) ..]) -> any
any doFor(any x) {
   any y, body, cond, a;
   cell c1;
   struct {  // bindFrame
      struct bindFrame *link;
      int i, cnt;
      struct {any sym; any val;} bnd[2];
   } f;

   f.link = Env.bind,  Env.bind = (bindFrame*)&f;
   f.i = 0;
   if (!isCell(y = car(x = cdr(x))) || !isCell(cdr(y))) {
      if (!isCell(y)) {
         f.cnt = 1;
         f.bnd[0].sym = y;
         f.bnd[0].val = val(y);
      }
      else {
         f.cnt = 2;
         f.bnd[0].sym = cdr(y);
         f.bnd[0].val = val(cdr(y));
         f.bnd[1].sym = car(y);
         f.bnd[1].val = val(car(y));
         val(f.bnd[1].sym) = Zero;
      }
      y = Nil;
      x = cdr(x),  Push(c1, EVAL(car(x)));
      if (isNum(data(c1)))
         val(f.bnd[0].sym) = Zero;
      body = x = cdr(x);
      for (;;) {
         if (isNum(data(c1))) {
            val(f.bnd[0].sym) = (any)(num(val(f.bnd[0].sym)) + 4);
            if (num(val(f.bnd[0].sym)) > num(data(c1)))
               break;
         }
         else {
            if (!isCell(data(c1)))
               break;
            val(f.bnd[0].sym) = car(data(c1));
            if (!isCell(data(c1) = cdr(data(c1))))
               data(c1) = Nil;
         }
         if (f.cnt == 2)
            val(f.bnd[1].sym) = (any)(num(val(f.bnd[1].sym)) + 4);
         do {
            if (!isNum(y = car(x))) {
               if (isSym(y))
                  y = val(y);
               else if (isNil(car(y))) {
                  y = cdr(y);
                  if (isNil(a = EVAL(car(y)))) {
                     y = prog(cdr(y));
                     goto for1;
                  }
                  val(At) = a;
                  y = Nil;
               }
               else if (car(y) == T) {
                  y = cdr(y);
                  if (!isNil(a = EVAL(car(y)))) {
                     val(At) = a;
                     y = prog(cdr(y));
                     goto for1;
                  }
                  y = Nil;
               }
               else
                  y = evList(y);
            }
         } while (isCell(x = cdr(x)));
         x = body;
      }
   for1:
      drop(c1);
      if (f.cnt == 2)
         val(f.bnd[1].sym) = f.bnd[1].val;
      val(f.bnd[0].sym) = f.bnd[0].val;
      Env.bind = f.link;
      return y;
   }
   if (!isCell(car(y))) {
      f.cnt = 1;
      f.bnd[0].sym = car(y);
      f.bnd[0].val = val(car(y));
   }
   else {
      f.cnt = 2;
      f.bnd[0].sym = cdar(y);
      f.bnd[0].val = val(cdar(y));
      f.bnd[1].sym = caar(y);
      f.bnd[1].val = val(caar(y));
      val(f.bnd[1].sym) = Zero;
   }
   y = cdr(y);
   val(f.bnd[0].sym) = EVAL(car(y));
   y = cdr(y),  cond = car(y),  y = cdr(y);
   Push(c1,Nil);
   body = x = cdr(x);
   for (;;) {
      if (f.cnt == 2)
         val(f.bnd[1].sym) = (any)(num(val(f.bnd[1].sym)) + 4);
      if (isNil(a = EVAL(cond)))
         break;
      val(At) = a;
      do {
         if (!isNum(data(c1) = car(x))) {
            if (isSym(data(c1)))
               data(c1) = val(data(c1));
            else if (isNil(car(data(c1)))) {
               data(c1) = cdr(data(c1));
               if (isNil(a = EVAL(car(data(c1))))) {
                  data(c1) = prog(cdr(data(c1)));
                  goto for2;
               }
               val(At) = a;
               data(c1) = Nil;
            }
            else if (car(data(c1)) == T) {
               data(c1) = cdr(data(c1));
               if (!isNil(a = EVAL(car(data(c1))))) {
                  val(At) = a;
                  data(c1) = prog(cdr(data(c1)));
                  goto for2;
               }
               data(c1) = Nil;
            }
            else
               data(c1) = evList(data(c1));
         }
      } while (isCell(x = cdr(x)));
      if (isCell(y))
         val(f.bnd[0].sym) = prog(y);
      x = body;
   }
for2:
   if (f.cnt == 2)
      val(f.bnd[1].sym) = f.bnd[1].val;
   val(f.bnd[0].sym) = f.bnd[0].val;
   Env.bind = f.link;
   return Pop(c1);
}
// (do 'flg|num ['any | (NIL 'any . prg) | (T 'any . prg) ..]) -> any
any doDo(any x) {
   any f, y, z, a;

   x = cdr(x);
   if (isNil(f = EVAL(car(x))))
      return Nil;
   if (isNum(f) && num(f) < 0)
      return Nil;
   x = cdr(x),  z = Nil;
   for (;;) {
      if (isNum(f)) {
         if (f == Zero)
            return z;
         f = (any)(num(f) - 4);
      }
      y = x;
      do {
         if (!isNum(z = car(y))) {
            if (isSym(z))
               z = val(z);
            else if (isNil(car(z))) {
               z = cdr(z);
               if (isNil(a = EVAL(car(z))))
                  return prog(cdr(z));
               val(At) = a;
               z = Nil;
            }
            else if (car(z) == T) {
               z = cdr(z);
               if (!isNil(a = EVAL(car(z)))) {
                  val(At) = a;
                  return prog(cdr(z));
               }
               z = Nil;
            }
            else
               z = evList(z);
         }
      } while (isCell(y = cdr(y)));
   }
}

///////////////////////////////////////////////
//               flow.c END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               gc.c START
///////////////////////////////////////////////


static void mark(any);

/* Mark data */
static void markTail(any x) {
   while (isCell(x)) {
      if (!(num(cdr(x)) & 1))
         return;
      *(long*)&cdr(x) &= ~1;
      mark(cdr(x)),  x = car(x);
   }
   if (!isTxt(x))
      do {
         if (!(num(val(x)) & 1))
            return;
         *(long*)&val(x) &= ~1;
      } while (!isNum(x = val(x)));
}

static void mark(any x) {
   while (isCell(x)) {
      if (!(num(cdr(x)) & 1))
         return;
      *(long*)&cdr(x) &= ~1;
      mark(car(x)),  x = cdr(x);
   }
   if (!isNum(x)  &&  num(val(x)) & 1) {
      *(long*)&val(x) &= ~1;
      mark(val(x));
      markTail(tail(x));
   }
}

/* Garbage collector */
static void gc(long c) {
   any p;
   heap *h;
   int i;

   h = Heaps;
   do {
      p = h->cells + CELLS-1;
      do
         *(long*)&cdr(p) |= 1;
      while (--p >= h->cells);
   } while (h = h->next);
   /* Mark */
   for (i = 0;  i < RAMS;  i += 2) {
      markTail(Ram[i]);
      mark(Ram[i+1]);
   }
   mark(Intern[0]),  mark(Intern[1]);
   mark(Transient[0]), mark(Transient[1]);
   mark(ApplyArgs),  mark(ApplyBody);
   for (p = Env.stack; p; p = cdr(p))
      mark(car(p));
   for (p = (any)Env.bind;  p;  p = (any)((bindFrame*)p)->link)
      for (i = ((bindFrame*)p)->cnt;  --i >= 0;) {
         mark(((bindFrame*)p)->bnd[i].sym);
         mark(((bindFrame*)p)->bnd[i].val);
      }
   for (p = (any)CatchPtr; p; p = (any)((catchFrame*)p)->link) {
      if (((catchFrame*)p)->tag)
         mark(((catchFrame*)p)->tag);
      mark(((catchFrame*)p)->fin);
   }
   /* Sweep */
   Avail = NULL;
   h = Heaps;
   if (c) {
      do {
         p = h->cells + CELLS-1;
         do
            if (num(p->cdr) & 1)
               Free(p),  --c;
         while (--p >= h->cells);
      } while (h = h->next);
      while (c >= 0)
         heapAlloc(),  c -= CELLS;
   }
   else {
      heap **hp = &Heaps;
      cell *av;

      do {
         c = CELLS;
         av = Avail;
         p = h->cells + CELLS-1;
         do
            if (num(p->cdr) & 1)
               Free(p),  --c;
         while (--p >= h->cells);
         if (c)
            hp = &h->next,  h = h->next;
         else
            Avail = av,  h = h->next,  free(*hp),  *hp = h;
      } while (h);
   }
}

/* Construct a cell */
any cons(any x, any y) {
   cell *p;

   if (!(p = Avail)) {
      cell c1, c2;

      Push(c1,x);
      Push(c2,y);
      gc(CELLS);
      drop(c1);
      p = Avail;
   }
   Avail = p->car;
   p->car = x;
   p->cdr = y;
   return p;
}

/* Construct a symbol */
any consSym(any val, word w) {
   cell *p;

   if (!(p = Avail)) {
      cell c1;

      if (!val)
         gc(CELLS);
      else {
         Push(c1,val);
         gc(CELLS);
         drop(c1);
      }
      p = Avail;
   }
   Avail = p->car;
   p = symPtr(p);
   val(p) = val ?: p;
   tail(p) = txt(w);
   return p;
}

/* Construct a name cell */
any consName(word w, any n) {
   cell *p;

   if (!(p = Avail)) {
      gc(CELLS);
      p = Avail;
   }
   Avail = p->car;
   p = symPtr(p);
   val(p) = n;
   tail(p) = (any)w;
   return p;
}
///////////////////////////////////////////////
//               gc.c END
///////////////////////////////////////////////



/*** System ***/
void giveup(char *msg) {
   fprintf(stderr, "%s\n", msg);
   exit(1);
}

void bye(int n) {
   exit(n);
}


/* Allocate memory */
void *alloc(void *p, size_t siz) {
   if (!(p = realloc(p,siz)))
      giveup("No memory");
   return p;
}

/* Allocate cell heap */
void heapAlloc(void) {
   heap *h;
   cell *p;

   h = (heap*)((long)alloc(NULL, sizeof(heap) + sizeof(cell)) + (sizeof(cell)-1) & ~(sizeof(cell)-1));
   h->next = Heaps,  Heaps = h;
   p = h->cells + CELLS-1;
   do
      Free(p);
   while (--p >= h->cells);
}

/*** Primitives ***/
any circ(any x) {
   any y;

   if (!isCell(x)  ||  x >= (any)Rom  &&  x < (any)(Rom+ROMS))
      return NULL;
   for (y = x;;) {
      any z = cdr(y);

      *(word*)&cdr(y) |= 1;
      if (!isCell(y = z)) {
         do
            *(word*)&cdr(x) &= ~1;
         while (isCell(x = cdr(x)));
         return NULL;
      }
      if (y >= (any)Rom  &&  y < (any)(Rom+ROMS)) {
         do
            *(word*)&cdr(x) &= ~1;
         while (y != (x = cdr(x)));
         return NULL;
      }
      if (num(cdr(y)) & 1) {
         while (x != y)
            *(word*)&cdr(x) &= ~1,  x = cdr(x);
         do
            *(word*)&cdr(x) &= ~1;
         while (y != (x = cdr(x)));
         return y;
      }
   }
}

/* Comparisons */
bool equal(any x, any y) {
   any a, b;
   bool res;

   if (x == y)
      return YES;
   if (isNum(x))
      return NO;
   if (isSym(x)) {
      if (!isSymb(y))
         return NO;
      if ((x = name(x)) == (y = name(y)))
         return x != txt(0);
      if (isTxt(x) || isTxt(y))
         return NO;
      do {
         if (num(tail(x)) != num(tail(y)))
            return NO;
         x = val(x),  y = val(y);
      } while (!isNum(x) && !isNum(y));
      return x == y;
   }
   if (!isCell(y))
      return NO;
   a = x, b = y;
   res = NO;
   for (;;) {
      if (!equal(car(x), (any)(num(car(y)) & ~1)))
         break;
      if (!isCell(cdr(x))) {
         res = equal(cdr(x), cdr(y));
         break;
      }
      if (!isCell(cdr(y)))
         break;
      if (x < (any)Rom  ||  x >= (any)(Rom+ROMS))
         *(word*)&car(x) |= 1;
      x = cdr(x),  y = cdr(y);
      if (num(car(x)) & 1) {
         for (;;) {
            if (a == x) {
               if (b == y) {
                  for (;;) {
                     a = cdr(a);
                     if ((b = cdr(b)) == y) {
                        res = a == x;
                        break;
                     }
                     if (a == x) {
                        res = YES;
                        break;
                     }
                  }
               }
               break;
            }
            if (b == y) {
               res = NO;
               break;
            }
            *(word*)&car(a) &= ~1,  a = cdr(a),  b = cdr(b);
         }
         do
            *(word*)&car(a) &= ~1,  a = cdr(a);
         while (a != x);
         return res;
      }
   }
   while (a != x  &&  (a < (any)Rom  ||  a >= (any)(Rom+ROMS)))
      *(word*)&car(a) &= ~1,  a = cdr(a);
   return res;
}


/*** Error handling ***/
void err(any ex, any x, char *fmt, ...) {
    bye(0);
    if (ex == x) bye(1);
    if (fmt == NULL) bye(1);
}


void argError(any ex, any x) {err(ex, x, "Bad argument");}
void numError(any ex, any x) {err(ex, x, "Number expected");}
void symError(any ex, any x) {err(ex, x, "Symbol expected");}
void pairError(any ex, any x) {err(ex, x, "Cons pair expected");}
void atomError(any ex, any x) {err(ex, x, "Atom expected");}
void lstError(any ex, any x) {err(ex, x, "List expected");}
void varError(any ex, any x) {err(ex, x, "Variable expected");}
void protError(any ex, any x) {err(ex, x, "Protected symbol");}

/*** Evaluation ***/
any evExpr(any expr, any x) {
   any y = car(expr);
   struct {  // bindFrame
      struct bindFrame *link;
      int i, cnt;
      struct {any sym; any val;} bnd[length(y)+2];
   } f;

   f.link = Env.bind,  Env.bind = (bindFrame*)&f;
   f.i = sizeof(f.bnd) / (2*sizeof(any)) - 1;
   f.cnt = 1,  f.bnd[0].sym = At,  f.bnd[0].val = val(At);
   while (isCell(y)) {
      f.bnd[f.cnt].sym = car(y);
      f.bnd[f.cnt].val = EVAL(car(x));
      ++f.cnt, x = cdr(x), y = cdr(y);
   }
   if (isNil(y)) {
      do {
         x = val(f.bnd[--f.i].sym);
         val(f.bnd[f.i].sym) = f.bnd[f.i].val;
         f.bnd[f.i].val = x;
      } while (f.i);
      x = prog(cdr(expr));
   }
   else if (y != At) {
      f.bnd[f.cnt].sym = y,  f.bnd[f.cnt++].val = val(y),  val(y) = x;
      do {
         x = val(f.bnd[--f.i].sym);
         val(f.bnd[f.i].sym) = f.bnd[f.i].val;
         f.bnd[f.i].val = x;
      } while (f.i);
      x = prog(cdr(expr));
   }
   else {
      int n, cnt;
      cell *arg;
      cell c[n = cnt = length(x)];

      while (--n >= 0)
         Push(c[n], EVAL(car(x))),  x = cdr(x);
      do {
         x = val(f.bnd[--f.i].sym);
         val(f.bnd[f.i].sym) = f.bnd[f.i].val;
         f.bnd[f.i].val = x;
      } while (f.i);
      n = Env.next,  Env.next = cnt;
      arg = Env.arg,  Env.arg = c;
      x = prog(cdr(expr));
      if (cnt)
         drop(c[cnt-1]);
      Env.arg = arg,  Env.next = n;
   }
   while (--f.cnt >= 0)
      val(f.bnd[f.cnt].sym) = f.bnd[f.cnt].val;
   Env.bind = f.link;
   return x;
}

void undefined(any x, any ex) {err(ex, x, "Undefined");}

static any evList2(any foo, any ex) {
   cell c1;

   Push(c1, foo);
   if (isCell(foo)) {
      foo = evExpr(foo, cdr(ex));
      drop(c1);
      return foo;
   }
   for (;;) {
      if (isNil(val(foo)))
         undefined(foo,ex);
      if (isNum(foo = val(foo))) {
         foo = evSubr(foo,ex);
         drop(c1);
         return foo;
      }
      if (isCell(foo)) {
         foo = evExpr(foo, cdr(ex));
         drop(c1);
         return foo;
      }
   }
}

/* Evaluate a list */
any evList(any ex) {
   any foo;

   if (isNum(foo = car(ex)))
      return ex;
   if (isCell(foo)) {
      if (isNum(foo = evList(foo)))
         return evSubr(foo,ex);
      return evList2(foo,ex);
   }
   for (;;) {
      if (isNil(val(foo)))
         undefined(foo,ex);
      if (isNum(foo = val(foo)))
         return evSubr(foo,ex);
      if (isCell(foo))
         return evExpr(foo, cdr(ex));
   }
}

any loadAll(any ex) {
   any x = Nil;

   while (*AV  &&  strcmp(*AV,"-") != 0)
      x = load(ex, 0, mkStr(*AV++));
   return x;
}

/*** Main ***/
int main(int ac, char *av[]) {
   int i;
   char *p;

   AV0 = *av++;
   AV = av;
   heapAlloc();
   Intern[0] = Intern[1] = Transient[0] = Transient[1] = Nil;
   intern(Nil, Intern);
   intern(T, Intern);
   intern(Meth, Intern);
   intern(Quote, Intern);  // Last protected symbol
   for (i = 1; i < RAMS; i += 2)
      if (Ram[i] != (any)(Ram + i))
         intern((any)(Ram + i), Intern);
   if (ac >= 2 && strcmp(av[ac-2], "+") == 0)
      val(Dbg) = T,  av[ac-2] = NULL;
   if (av[0] && *av[0] != '-' && (p = strrchr(av[0], '/')) && !(p == av[0]+1 && *av[0] == '.')) {
      Home = malloc(p - av[0] + 2);
      memcpy(Home, av[0], p - av[0] + 1);
      Home[p - av[0] + 1] = '\0';
   }
   InFile = stdin,  Env.get = getStdin;
   OutFile = stdout,  Env.put = putStdout;
   ApplyArgs = cons(cons(consSym(Nil,0), Nil), Nil);
   ApplyBody = cons(Nil,Nil);
   if (!setjmp(ErrRst))
      prog(val(Main)),  loadAll(NULL);
   while (!feof(stdin))
      load(NULL, ':', Nil);
   bye(0);
}
