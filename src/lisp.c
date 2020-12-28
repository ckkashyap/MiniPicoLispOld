#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>

#ifndef CELLS
//#define CELLS (1*sizeof(cell))
#define CELLS 48 
#endif

//int CELLS = 48;

#define WORD ((int)sizeof(long long))
#define BITS (8*WORD)

typedef unsigned long long word; // TODO - THIS IS USED IN SUBTRACTION !!!!
typedef unsigned char byte;
typedef unsigned char *ptr;

#undef bool
typedef enum {NO,YES} bool;

typedef union
{
    unsigned char parts[4];
    word _t;
} PartType;
typedef struct cell {            // PicoLisp primary data type
   struct cell *car;
   struct cell *cdr;
   PartType type;
} cell, *any;

typedef any (*fun)(any);

typedef enum
{
    UNDEFINED,
    TXT,
    NUM,
    FUNC,
    PTR_CELL,
    INTERN,
} CellPartType;

CellPartType getCARType(any cell)
{
    return cell->type.parts[0];
}

CellPartType getCDRType(any cell)
{
    return cell->type.parts[1];
}

void setCARType(any cell, CellPartType type)
{
    cell->type.parts[0] = type;
}

void setCDRType(any cell, CellPartType type)
{
    cell->type.parts[1] = type;
}

void setList(any cell)
{
    cell->type.parts[2] = 1;
}

int isList(any cell)
{
    return cell->type.parts[2];
}

void setMark(any cell, int m)
{
    cell->type.parts[3] = m;
}

int getMark(any cell)
{
    return cell->type.parts[3];
}



//////////////////////////////////////////////////////////
// TODO this has to be fixed
#define Quote (any)(Rom+7)
any doQuote(any);

#define ROMS 18
#define RAMS 34
//////////////////////////////////////////////////////////

#include "def.d"
#include "mem.d"

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


typedef struct {
        any sym; any val;
} bindFrameBind;
#define bindFrameSize (sizeof(bindFrame))
#define bindSize (sizeof(bindFrameBind))
static inline bindFrame *allocFrame(int l)
{
    int s1 = bindFrameSize;
    int s2 = (l - 1) * bindSize;
    return (bindFrame*)malloc(s1 + s2);
};


/*** Macros ***/
#define Free(p)         ((p)->car=Avail, (p)->cdr=0, (p)->type._t=0,  Avail=(p))

/* Number access */
#define num(x)          ((long long)(x))
#define txt(n)          ((any)(num(n)<<1|1))
#define box(n)          ((any)(num(n)<<2|2))
//#define unBox(n)        (num(n)>>2)
#define unBox(n)        (num(n->car))
//#define Zero            ((any)2)
#define One             ((any)6)

/* Symbol access */
//#define symPtr(x)       ((any)&(x)->cdr)
#define symPtr(x)       (x)
//#define val(x)          ((x)->car)
#define val(x)          ((x)->cdr)


any tail1(any x)
{
   return (((x)-1)->cdr);
}
#define tail(x)         (x)

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
// isTxt should just check if car is txt
//#define isTxt(x)        (num(x)&1)
#define isTxt(x)        (((any)(x))->type.parts[0] == TXT)
//#define isNum(x)        (num(x)&2)
#define isNum(x)        (((any)(x))->type.parts[0] == NUM)
//#define isSym(x)        (num(x)&WORD)
bool isSym(any x)
{
   if (x) return 0;
   // TODO - this must be checked out
   return 0;
}
#define isSymb(x)       ((num(x)&(WORD+2))==WORD)
//#define isCell(x)       (!(num(x)&(2*WORD-1)))
#define isCell(x)        (((any)(x))->type.parts[0] == PTR_CELL)
#define isFunc(x)        (((any)(x))->type.parts[1] == FUNC)

/* Evaluation */
any evList(any);
/* Evaluation */
any EVAL(any x)
{
   if (isNum(x))
   {
      return x;
   }
   //else if (isSym(x))
   else if (isTxt(x))
   {
      return val(x);
   }
   else
   {
      return evList(x);
   }
}
#define evSubr(f,x)     (*(fun)(num(f)))(x)

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

static void gc(long long c);
word getHeapSize();
/* Prototypes */
void *alloc(void*,size_t);
any apply(any,any,bool,int,cell*);
void argError(any,any) ;
void atomError(any,any) ;
void begString(void);
void brkLoad(any);
int bufNum(char[BITS/2],long long);
int bufSize(any);
void bufString(any,char*);
void bye(int) ;
void pairError(any,any) ;
any circ(any);
long long compare(any,any);
any consIntern(any,any);
any cons(any,any);
any consName(word,any);
any consSym(any,word);
void newline(void);
any endString(void);
bool equal(any,any);
void err(any,any,char*,...) ;
any evExpr(any,any);
any evList(any);
long long evNum(any,any);
any evSym(any);
void execError(char*) ;
int firstByte(any);
any get(any,any);
int getByte(int*,word*,any*);
int getByte1(int*,word*,any*);
void getStdin(void);
void giveup(char*) ;
void heapAlloc(void);
any intern(any,any[2]);
any isIntern(any,any[2]);
void lstError(any,any) ;
any load(any,int,any);
any loadAll(any);
any method(any);
any mkChar(int);
any mkSym(byte*);
any mkStr(char*);
any mkTxt(int);
any name(any);
void numError(any,any) ;
any numToSym(any,int,int,int);
void outName(any);
void outNum(long long);
void outString(char*);
void pack(any,int*,word*,any*,cell*);
int pathSize(any);
void pathString(any,char*);
void popInFiles(void);
void popOutFiles(void);
any popSym(int,word,any,cell*);
void prin(any);
void print(any);
void protError(any,any) ;
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
void symError(any,any) ;
any symToNum(any,int,int,int);
void undefined(any,any);
void unintern(any,any[2]);
void unwind (catchFrame*);
void varError(any,any) ;
void wrOpen(any,any,outFrame*);
long long xNum(any,any);
any xSym(any);

/* List length calculation */
static inline int length(any x) {
   int n;

   //for (n = 0; isCell(x); x = cdr(x))
   for (n = 0; x != Nil; x = cdr(x))
      ++n;
   return n;
}

/* List interpreter */
static inline any prog(any x) {
   any y;

   do
      y = EVAL(car(x));
   while (Nil != (x = cdr(x)));
   //while (isCell(x = cdr(x)));
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

//////////////////////////////////////////////////////////// TODO THIS SHOULD BE REMOVED
/* ROM Data */
any const Rom[] = { (any)0 };

/* RAM Symbols */
any Ram[] = { (any)0 };

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

   *i = BITS - 1, *p = (word)*q , *q = NULL;

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
         //tail(data(*cp)) = *q = consName(*p, Zero); // TODO WHATS GOING ON
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
      //tail(data(*cp)) = consName(n, Zero); // TODO WHATS GOING ON
      return Pop(*cp);
   }
   return consSym(NULL,n);
}

int symBytes(any x) {
   int cnt = 0;
   word w;

   if (isNil(x))
      return 0;

   if (isTxt(x)) {
      w = (word)(x->car);
      while (w)
         ++cnt,  w >>= 8;
   }

   return cnt;
}

any isIntern(any nm, any tree[2]) {
   any x;
   long long n;

   if (isTxt(nm)) {
      //for (x = tree[0];  isCell(x);) {
      for (x = tree[0];  x != Nil;) {
         //if ((n = (word)nm - (word)name(car(x))) == 0)
         //if ((n = (long long)nm - (long long)name(car(x))) == 0)
         if ((n = (long long)(car(nm)) - (long long)name(caar(x))) == 0)
            return car(x);
         x = n<0? cadr(x) : cddr(x);
      }
   }

   return NULL;
}

any intern(any sym, any tree[2])
{
   any nm, x;
   long long n;

   nm = sym;
   //if ((nm = name(sym)) == txt(0))
   //   return sym;

   x = tree[0];
   if (Nil == x)
   {
      tree[0] = consIntern(sym, Nil);
      return sym;
   }
   for (;;)
   {
      if ((n = (long long)(car(nm)) - (long long)name(caar(x))) == 0)
         return car(x);

      //if (!isCell(cdr(x)))
      if (Nil == cdr(x))
      {
         cdr(x) = n < 0 ? consIntern(consIntern(sym, Nil), Nil) : consIntern(Nil, consIntern(sym, Nil));
         return sym;
      }
      if (n < 0)
      {
         if (Nil != cadr(x))
         //if (isCell(cadr(x)))
            x = cadr(x);
         else
         {
            cadr(x) = consIntern(sym, Nil);
            return sym;
         }
      }
      else
      {
         if (Nil != cddr(x))
         //if (isCell(cddr(x)))
            x = cddr(x);
         else
         {
            cddr(x) = consIntern(sym, Nil);
            return sym;
         }
      }
   }
}

void unintern(any sym, any tree[2]) {
   any nm, x, y, z, *p;
   long long n;

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
   //for (s = tail(s); isCell(s); s = car(s));
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
any mkStr(char *s)
{
   if (s && *s)
   {
      return mkSym((byte *)s);
   }
   else
   {
      return Nil;
   }
}

// (==== ['sym ..]) -> NIL
any doHide(any ex) {
   any x, y;

   Transient[0] = Transient[1] = Nil;
   for (x = cdr(ex); x != Nil; x = cdr(x)) {
      y = EVAL(car(x));
      NeedSymb(ex,y);
      intern(y, Transient);
   }
   return Nil;
}

// (c...r 'lst) -> any
any doCar(any ex) {
   any x = cdr(ex);
   x = EVAL(car(x));
   NeedLst(ex,x);
   return car(x);
}

any doCdr(any ex) {
   any x = cdr(ex);
   x = EVAL(car(x));
   NeedLst(ex,x);
   return cdr(x);
}

any doCons(any x) {
   any y;
   cell c1;

   x = cdr(x);
   Push(c1, y = cons(EVAL(car(x)),Nil));
   while (Nil != (cdr(x = cdr(x))))
      y = cdr(y) = cons(EVAL(car(x)),Nil);
   cdr(y) = EVAL(car(x));
   doDump(Nil);
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
   //} while (isCell(x = cdr(x)));
   } while (Nil != (x = cdr(x)));
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
   } while (Nil != (x = cdr(x)));
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
   //while (isCell(x = cdr(x)))
   while (Nil != (x = cdr(x)))
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

   x = x->car;
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
   //NeedSymb(ex,x); // TODO WHAT IS THIS ABOUT?
   if (isNil(x))
      f->fp = stdin;
   else {
      int ps = pathSize(x);
      // TODO - check what can be done for stack FREE MUST BE ADDED
      //char nm[ps];
      char *nm = (char*)malloc(ps);

      pathString(x,nm);
      if (nm[0] == '+') {
         if (!(f->fp = fopen(nm+1, "a+")))
            openErr(ex, nm);
         fseek(f->fp, 0L, SEEK_SET);
      }
      else if (!(f->fp = fopen(nm, "r")))
         openErr(ex, nm);

      free(nm);
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
   unsigned long long n;
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
      return cons(doQuote_D, read0(top));
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

   int count=0;
   for (;;)
   {
       count++;
       if (count > 6)
       {
           printf("%s too long\n", &w);
           bye(0);
       }
       Env.get();
       if (strchr(Delim, Chr))
       {
           break;
       }
       if (Chr == '\\')
       {
           Env.get();
       }
       putByte(Chr, &i, &w, &p, &c1);
   }

   y = popSym(i, w, p, &c1);
   if (x = symToNum(tail(y), 0, '.', 0))
   {
      return x;
   }
   if (x = anonymous(name(y)))
   {
      return x;
   }
   if (x = isIntern(tail(y), Intern))
   {
      return x;
   }

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
      return symToNum(tail(popSym(i, w, p, &c1)), 0, '.', 0);
   }
   if (Chr != '+' && Chr != '-') {
      // TODO check what needs to be done about stack - FREE MUST BE ADDED
      // char nm[bufSize(x)];
      char *nm = (char *)malloc(bufSize(x));

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
         {
             free(nm);
            return x;
         }
         intern(y, Intern);
         val(y) = Nil;
         free(nm);
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
          // TODO - WHY @ does not work in files
         x = EVAL(data(c1));
      else {
         Push(c2, val(At));
         x = val(At) = EVAL(data(c1));
         val(At3) = val(At2),  val(At2) = data(c2);
         outString("-> "),  fflush(OutFile),  print(x),  newline();
         getHeapSize();
         gc(CELLS);
         getHeapSize();
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

int bufNum(char buf[BITS/2], long long n) {
   return sprintf(buf, "%ld", n);
}

void outNum(long long n) {
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
   if (x == T)
   {
      printf("T");
      return;
   }

   if (x == Nil)
   {
      printf("Nil");
      return;
   }

   if (isNum(x))
   {
      outNum(unBox(x));
      return;
   }
   printf ("TODO NOT A NUMBER %p %p\n", x, Nil);
   return;

   if (isSym(x)) {
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
    if (x == Nil)
    {
         printf("T");
         return;
    }

   if (!isNil(x)) {
      if (isNum(x))
         outNum(unBox(x));
      else if (x==T) {
         printf("T");
      }
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

   //while (isCell(x = cdr(x)))
   while (Nil != (x = cdr(x)))
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


/* Make number from symbol */
any symToNum(any sym, int scl, int sep, int ign) {
   unsigned c;
   int i;
   word w;
   bool sign, frac;
   long long n;
   any s = sym->car;


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
   //return box(sign? -n : n);


   any r = cons((any)n, Nil);
   r->type.parts[0] = NUM;
   
   return r;
}

// (+ 'num ..) -> num
any doAdd(any ex) {
   any x, y;
   long long n=0;

   x = cdr(ex);
   if (isNil(y = EVAL(car(x))))
      return Nil;
   NeedNum(ex,y);
   n = unBox(y);
   while (Nil != (x = cdr(x))) {
      if (isNil(y = EVAL(car(x))))
         return Nil;
      NeedNum(ex,y);
      n += unBox(y);
   }

   any r = cons((any)n, Nil);
   r->type.parts[0] = NUM;
   return r;
}

any doSub(any ex) {
   any x, y;
   long long n=0;

   x = cdr(ex);
   if (isNil(y = EVAL(car(x))))
      return Nil;
   NeedNum(ex,y);
   n = unBox(y);
   while (Nil != (x = cdr(x))) {
      if (isNil(y = EVAL(car(x))))
         return Nil;
      NeedNum(ex,y);
      n -= unBox(y);
   }

   any r = cons((any)n, Nil);
   r->type.parts[0] = NUM;
   return r;
}

any doMul(any ex) {
   any x, y;
   long long n=0;

   x = cdr(ex);
   if (isNil(y = EVAL(car(x))))
      return Nil;
   NeedNum(ex,y);
   n = unBox(y);
   while (Nil != (x = cdr(x))) {
      if (isNil(y = EVAL(car(x))))
         return Nil;
      NeedNum(ex,y);
      n *= unBox(y);
   }

   any r = cons((any)n, Nil);
   r->type.parts[0] = NUM;
   return r;
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
   //NeedSymb(ex,s); TODO - GOTTA KNOW WHAT"S GOING ON HERE
   //CheckVar(ex,s);

   if (ex == Nil)
   {
      giveup("THIS SHOULD NOT HAPPEN");
   }

   if (!isNil(val(s))  &&  s != val(s)  &&  !equal(x,val(s)))
      redefMsg(s,NULL);
   val(s) = x;

   setCDRType(s, PTR_CELL); // TODO - DO IT MORE NEATLY
}

// (quote . any) -> any
any doQuote(any x) {return cdr(x);}

// (== 'any ..) -> flg
any doEq(any x) {
   cell c1;

   x = cdr(x),  Push(c1, EVAL(car(x)));
   while (Nil != (x = cdr(x)))
      //if (data(c1) != EVAL(car(x))) { // TODO CHECK IT OUT
      if (car(data(c1)) != car(EVAL(car(x)))) {
         drop(c1);
         return Nil;
      }
   drop(c1);
   return T;
}

// (if 'any1 any2 . prg) -> any
any doIf(any x) {
   any a;

   x = cdr(x);
   if (isNil(a = EVAL(car(x))))
      return prog(cddr(x));
   val(At) = a;
   x = cdr(x);
   return EVAL(car(x));
}

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
/*
 * Bind essentially backs up the symbols
 * For example if you have 
 * (setq X 10)
 * (let (X 20) X)
 * In this case the value 10 of X should be backed up
 * and restored after the let binding
 *
 */
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
       // TODO check out how to do stack 
       bindFrame *f = allocFrame((length(y)+1)/2);

      f->link = Env.bind,  Env.bind = f;
      f->i = f->cnt = 0;
      do {
         f->bnd[f->cnt].sym = car(y);
         f->bnd[f->cnt].val = val(car(y));
         ++f->cnt;
         val(car(y)) = EVAL(cadr(y));
      } while (isCell(y = cddr(y)) && y != Nil);
      x = prog(cdr(x));
      while (--f->cnt >= 0)
         val(f->bnd[f->cnt].sym) = f->bnd[f->cnt].val;
      Env.bind = f->link;

      free(f);
   }
   return x;
}

// (bye 'num|NIL)
any doBye(any ex) {
   printf("\n");
   bye(0);
   return ex;
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
         //if (f == Zero)
         if (f->car == 0)
            return z;
         f->car = (any)((word)f->car - 1);
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
      //} while (isCell(y = cdr(y)));
      } while (Nil != (y = cdr(y)));
   }
}

///////////////////////////////////////////////
//               flow.c END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               gc.c START
///////////////////////////////////////////////


static void mark(any);

int MARKER = 0;

static void mark(any x) {
    if (!x) return;

    if (getMark(x)) return;

    setMark(x, MARKER);

    if (x == Nil) return;

    if (getCARType(x) == PTR_CELL || getCARType(x) == INTERN) mark(car(x));

    while (1)
    {
        if (getCDRType(x) != PTR_CELL && getCARType(x) != INTERN) break;
        x = cdr(x);
        if (!x) break;
        if (x==Nil) break;
        if (getMark(x)) break;
        setMark(x, MARKER);
        if (getCARType(x) == PTR_CELL || getCARType(x) == INTERN) mark(car(x));
    }
    //if (getCDRType(x) == PTR_CELL || getCARType(x) == INTERN) mark(cdr(x));
}

void dump(FILE *fp, any p)
{

    if (getCARType(p) == TXT)
    {
        fprintf(fp, "%p %s(TXT = %p) %p %p\n", p, &(p->car),p->car, p->cdr, p->type._t);
    }
    else
    {
        fprintf(fp, "%p ", p);
        if(p->car) fprintf(fp, "%p ", p->car); else fprintf(fp, "0 ");
        if(p->cdr) fprintf(fp, "%p ", p->cdr); else fprintf(fp, "0 ");
        if(p->type._t) fprintf(fp, "%p\n", p->type._t); else fprintf(fp, "0\n");
        //fprintf(fp, "0x%016lx %p %p %p\n", p, p->car, p->cdr, p->type._t);
    }
}

void sweep(int free)
{
   any p;
   heap *h;
   int i, c =100;
   /* Sweep */
   if(free)Avail = NULL;
   h = Heaps;
   if (c) {
      do {
         p = h->cells + CELLS-1;
         do
         {
            if (!getMark(p))
            {
                printf("FREEING %p  .... \n", p);
                if (free) Free(p);
               --c;
            }
            if(free)setMark(p, 0);
         }
         while (--p >= h->cells);
      } while (h = h->next);

      //while (c >= 0)
      //{
      //   heapAlloc(),  c -= CELLS;
      //}
   }

   printf("AVAIL = %p\n", Avail);
}


void dumpHeaps(FILE *mem, heap *h)
{
    any p;
    if (!h) return;
    dumpHeaps(mem, h->next);

    fprintf(mem, "# START HEAP\n");
    p = h->cells + CELLS-1;
    do
    {
        //fprintf(mem, "0x%016lx %p %p %p\n", p, p->car, p->cdr, p->type._t);
        dump(mem, p);
    }
    while (--p >= h->cells);
}

any doDump(any ignore)
{
    return ignore;
    static int COUNT=0;
    char debugFileName[100];
    sprintf(debugFileName, "debug-%03d.mem", COUNT++);
    if (T == cadr(ignore))
    {
        markAll();
        sweep(0);
    }
    if ( 0 == car(cadr(ignore)))
    {
        markAll();
        sweep(1);
    }

    if ( 0 == car(cadr(ignore)))
    {
        gc(CELLS);
    }

    FILE *mem;
    mem = fopen(debugFileName, "w");

    fprintf(mem, "# START MEM\n");
    for (int i = 0; i < MEMS; i += 3)
    {
        //fprintf(mem, "0x%016lx %p %p %p\n", &Mem[i], Mem[i], Mem[i + 1], Mem[i + 2]);
        dump(mem, (any)(&Mem[i]));
    }

    heap *h = Heaps;
    any p;

    dumpHeaps(mem, h);
    // do
    // {
    //     fprintf(mem, "# START HEAP\n");
    //     p = h->cells + CELLS-1;
    //     do
    //     {
    //         //fprintf(mem, "0x%016lx %p %p %p\n", p, p->car, p->cdr, p->type._t);
    //         dump(mem, p);
    //     }
    //     while (--p >= h->cells);
    // } while (h = h->next);

    fclose(mem);

    return Nil;
}

void markAll()
{
   any p;
   int i;

MARKER = 1;
   for (i = 0; i < MEMS; i += 3)
   {
       mark(&Mem[i]);
   }

MARKER = 2;
   /* Mark */
   mark(Intern[0]);
MARKER = 3;
   mark(Transient[0]);
MARKER = 4;
   mark(ApplyArgs);
MARKER = 5;
   mark(ApplyBody);
MARKER = 6;
   for (p = Env.stack; p; p = cdr(p))
   {
      mark(car(p));
   }
MARKER = 7;
   for (p = (any)Env.bind;  p;  p = (any)((bindFrame*)p)->link)
   {
      for (i = ((bindFrame*)p)->cnt;  --i >= 0;)
      {
         mark(((bindFrame*)p)->bnd[i].sym);
         mark(((bindFrame*)p)->bnd[i].val);
      }
   }
MARKER = 8;
   for (p = (any)CatchPtr; p; p = (any)((catchFrame*)p)->link) {
       printf("Marking catch frames\n");
      if (((catchFrame*)p)->tag)
         mark(((catchFrame*)p)->tag);
      mark(((catchFrame*)p)->fin);
   }
}

word getHeapSize()
{
    word size = 0;
    word sizeFree = 0;
    heap *h = Heaps;
    do {
        any p = h->cells + CELLS-1;
        do
        {
            size++;
        }
        while (--p >= h->cells);
    } while (h = h->next);

    any p = Avail;
    while (p)
    {
        sizeFree++;
        p = car(p);
    }

    printf("MEM SIZE = %lld FREE = %lld Nil = %p\n", size, sizeFree, Nil);

    return size;
}

/* Garbage collector */
static void gc(long long c) {
   any p;
   heap *h;
   int i;


   printf("GC CALLED\n");
   doDump(Nil);


// MARKER = 1;
//    for (int i = 0; i < MEMS; i += 3)
//    {
//        mark(&Mem[i]);
//    }
// 
// MARKER = 2;
//    /* Mark */
//    mark(Intern[0]);
// MARKER = 3;
//    mark(Transient[0]);
// MARKER = 4;
//    mark(ApplyArgs);
// MARKER = 5;
//    mark(ApplyBody);
// MARKER = 6;
//    for (p = Env.stack; p; p = cdr(p))
//    {
//       mark(car(p));
//    }
// MARKER = 7;
//    for (p = (any)Env.bind;  p;  p = (any)((bindFrame*)p)->link)
//    {
//       for (i = ((bindFrame*)p)->cnt;  --i >= 0;)
//       {
//          mark(((bindFrame*)p)->bnd[i].sym);
//          mark(((bindFrame*)p)->bnd[i].val);
//       }
//    }
// MARKER = 8;
//    for (p = (any)CatchPtr; p; p = (any)((catchFrame*)p)->link) {
//        printf("Marking catch frames\n");
//       if (((catchFrame*)p)->tag)
//          mark(((catchFrame*)p)->tag);
//       mark(((catchFrame*)p)->fin);
//    }

   markAll();
   doDump(Nil);




   /* Sweep */
   Avail = NULL;
   h = Heaps;
   if (c) {
      do {
         p = h->cells + CELLS-1;
         do
         {
            if (!getMark(p))
            {
                //printf("Freeing %p\n", p);
                Free(p);
               --c;
            }
            else
            {
                //printf("Keeping %p\n", p);
            }
            setMark(p, 0);
         }
         while (--p >= h->cells);
      } while (h = h->next);


      printf("C = %lld\n", c);

      while (c >= 0)
      {
         heapAlloc(),  c -= CELLS;
      }
   }

//heapAlloc();
//
   printf("GC RETURNING AVAIL=%p\n", Avail);
   doDump(Nil);
   return;
}

any consIntern(any x, any y) {
    any r = cons(x, y);

   setCARType(r, INTERN);
   setCDRType(r, INTERN);

   return r;
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
   setCARType(p, PTR_CELL);
   setCDRType(p, PTR_CELL);
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
   p->cdr = val ? val : p;
   p->car = (any)w;
   p->type.parts[0] = TXT;
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
   p->car = (any)w;
   p->cdr = n;
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


   printf("HEAP ALLOC CALLED\n");

   h = (heap*)((long long)alloc(NULL, sizeof(heap) + sizeof(cell)) + (sizeof(cell)-1) & ~(sizeof(cell)-1));
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
   printf("ERROR\n");
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
any evExpr(any expr, any x)
{
   any y = car(expr);

   bindFrame *f = allocFrame(length(y)+2);

   f->link = Env.bind,  Env.bind = f;
   f->i = (bindSize * (length(y)+2)) / (2*sizeof(any)) - 1;
   f->cnt = 1,  f->bnd[0].sym = At,  f->bnd[0].val = val(At);
   while (y != Nil)
   {
      f->bnd[f->cnt].sym = car(y);
      f->bnd[f->cnt].val = EVAL(car(x));
      ++f->cnt, x = cdr(x), y = cdr(y);
   }

   if (isNil(y)) {
      do
      {
         x = val(f->bnd[--f->i].sym);
         val(f->bnd[f->i].sym) = f->bnd[f->i].val;
         f->bnd[f->i].val = x;
      }
      while (f->i);

      x = prog(cdr(expr));
   }
   else if (y != At)
   {
      f->bnd[f->cnt].sym = y,  f->bnd[f->cnt++].val = val(y),  val(y) = x;
      do
      {
         x = val(f->bnd[--f->i].sym);
         val(f->bnd[f->i].sym) = f->bnd[f->i].val;
         f->bnd[f->i].val = x;
      }
      while (f->i);
      x = prog(cdr(expr));
   }
   else
   {
      int n, cnt;
      cell *arg;
      cell *c = (cell*)malloc(sizeof(cell) * (n = cnt = length(x)));

      while (--n >= 0)
      {
         Push(c[n], EVAL(car(x))),  x = cdr(x);
      }

      do
      {
         x = val(f->bnd[--f->i].sym);
         val(f->bnd[f->i].sym) = f->bnd[f->i].val;
         f->bnd[f->i].val = x;
      }
      while (f->i);

      n = Env.next,  Env.next = cnt;
      arg = Env.arg,  Env.arg = c;
      x = prog(cdr(expr));
      if (cnt)
      {
         drop(c[cnt-1]);
      }

      Env.arg = arg,  Env.next = n;
      free(c);
   }

   while (--f->cnt >= 0)
   {
      val(f->bnd[f->cnt].sym) = f->bnd[f->cnt].val;
   }

   Env.bind = f->link;
   free(f);
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
      if (isFunc(foo))
         return evSubr(foo->cdr,ex);
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

void printTXT(any cell)
{
        word w = (word)cell->car;

        printf("<");
        for(int i = 0; i < 8; i++)
        {
            word c = w & (word)0xff00000000000000;
            c >>= 56;
            if (!c)
            {
                w <<= 8;
                continue;
            }
            printf("%c", (char)c);
            w <<= 8;
        }
        printf(">");
}

void printNUM(any cell)
{
    printf("%lld", (long long)cell->car);
}
void printCell(any cell)
{
    if (cell == Nil)
    {
        printf("Nil");
        return;
    }

    CellPartType carType = getCARType(cell);

    if (carType == TXT)
    {
        printTXT(cell);
    }
    else if (carType == NUM)
    {
        printNUM(cell);
    }

    if (isList(cell))
    {
        printf("(");
        while(isList(cell))
        {
            carType = getCARType(cell);
            if (carType == TXT) printTXT(cell);
            else if (carType == NUM) printNUM(cell);
            else printCell(cell->car);
            cell = cell->cdr;
            printf(" ");
        }
        printf(")");
    }
}

/*** Main ***/
int main(int ac, char *av[])
{
   if (ac == 0) printf("STRANGE\n");

   av++;
   AV = av;
   heapAlloc();
   doDump(Nil);
   getHeapSize();
   //CELLS = 1;
   Intern[0] = Intern[1] = Transient[0] = Transient[1] = Nil;

   Mem[4] = (any)Mem; // TODO - SETTING THE VALUE OF NIL
   Mem[7] = (any)(Mem+6); // TODO - SETTING THE VALUE OF NIL

   //intern(Nil, Intern);
   //isIntern(Nil, Intern);
   for (int i = 3; i < MEMS; i += 3) // 2 because Nil has already been interned
   {
      any cell = (any)&Mem[i];
      CellPartType carType = getCARType(cell);
      CellPartType cdrType = getCDRType(cell);

      //printf("%d %d\n", GetCARType(cell), GetCDRType(cell));
      if (TXT == carType && cdrType != FUNC && cell->cdr)
      {
         //printf("%d\n", i);
         intern(cell, Intern);
         //printCell(cell);
         //printCell(cell->cdr);
         //printf("\n");
      }
      else if (TXT == carType && cdrType == FUNC && cell->cdr)
      {
         //printf("%d\n", i);
         intern(cell, Intern);
         //printCell(cell);
         //printf(" CFUNC\n");
      }
      else if (TXT == carType)
      {
         intern(cell, Intern);
      }
   }

   InFile = stdin, Env.get = getStdin;
   OutFile = stdout, Env.put = putStdout;
   ApplyArgs = cons(cons(consSym(Nil, 0), Nil), Nil);
   ApplyBody = cons(Nil, Nil);

   doDump(Nil);
   getHeapSize();
   loadAll(NULL);
   while (!feof(stdin))
      load(NULL, ':', Nil);
   bye(0);
}
