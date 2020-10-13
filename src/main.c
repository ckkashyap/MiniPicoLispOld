/* 12jul15abu
 * (c) Software Lab. Alexander Burger
 */

#include "pico.h"

/* Globals */
int Chr, Trace;
char **AV, *AV0, *Home;
heap *Heaps;
cell *Avail;
stkEnv Env;
catchFrame *CatchPtr;
FILE *InFile, *OutFile;
any TheKey, TheCls, Thrown;
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

static bool Jam;
static jmp_buf ErrRst;


///////////////////////////////////////////////
//               sym.c
///////////////////////////////////////////////

static byte Ascii6[] = {
   0,  2,  2,  2,  2,  2,  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,  2,  2,  2,  2,  2,  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,  1,  3,  5,  7,  9, 11,  13,  15,  17,  19,  21,  23,  25,   4,   6,
  27, 29, 31, 33, 35, 37, 39,  41,  43,  45,  47,  49,   8,  51,  10,  53,
  55, 57, 59, 61, 63, 65, 67,  69,  71,  73,  75,  77,  79,  81,  83,  85,
  87, 89, 91, 93, 95, 97, 99, 101, 103, 105, 107, 109, 111, 113, 115, 117,
 119, 12, 14, 16, 18, 20, 22,  24,  26,  28,  30,  32,  34,  36,  38,  40,
  42, 44, 46, 48, 50, 52, 54,  56,  58,  60,  62, 121, 123, 125, 127,   0
};

static byte Ascii7[] = {
   0, 33,  32, 34,  46, 35,  47, 36,  60,  37,  62,  38,  97,  39,  98,  40,
  99, 41, 100, 42, 101, 43, 102, 44, 103,  45, 104,  48, 105,  49, 106,  50,
 107, 51, 108, 52, 109, 53, 110, 54, 111,  55, 112,  56, 113,  57, 114,  58,
 115, 59, 116, 61, 117, 63, 118, 64, 119,  65, 120,  66, 121,  67, 122,  68,
   0, 69,   0, 70,   0, 71,   0, 72,   0,  73,   0,  74,   0,  75,   0,  76,
   0, 77,   0, 78,   0, 79,   0, 80,   0,  81,   0,  82,   0,  83,   0,  84,
   0, 85,   0, 86,   0, 87,   0, 88,   0,  89,   0,  90,   0,  91,   0,  92,
   0, 93,   0, 94,   0, 95,   0, 96,   0, 123,   0, 124,   0, 125,   0, 126
};


int firstByte(any s) {
   int c;

   if (isNil(s))
      return 0;
   c = (int)(isTxt(s = name(s))? (word)s >> 1 : (word)tail(s));
   return Ascii7[c & (c & 1? 127 : 63)];
}

int secondByte(any s) {
   int c;

   if (isNil(s))
      return 0;
   c = (int)(isTxt(s = name(s))? (word)s >> 1 : (word)tail(s));
   c >>= c & 1? 7 : 6;
   return Ascii7[c & (c & 1? 127 : 63)];
}

int getByte1(int *i, word *p, any *q) {
   int c;

   if (isTxt(*q))
      *i = BITS-1,  *p = (word)*q >> 1,  *q = NULL;
   else
      *i = BITS,  *p = (word)tail(*q),  *q = val(*q);
   if (*p & 1)
      c = Ascii7[*p & 127],  *p >>= 7,  *i -= 7;
   else
      c = Ascii7[*p & 63],  *p >>= 6,  *i -= 6;
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
   if (*p & 1) {
      c = *p & 127,  *p >>= 7;
      if (*i >= 7)
         *i -= 7;
      else if (isNum(*q)) {
         *p = (word)*q >> 2,  *q = NULL;
         c |= *p << *i;
         *p >>= 7 - *i;
         *i += BITS-9;
      }
      else {
         *p = (word)tail(*q),  *q = val(*q);
         c |= *p << *i;
         *p >>= 7 - *i;
         *i += BITS-7;
      }
      c &= 127;
   }
   else {
      c = *p & 63,  *p >>= 6;
      if (*i >= 6)
         *i -= 6;
      else if (!*q)
         return 0;
      else if (isNum(*q)) {
         *p = (word)*q >> 2,  *q = NULL;
         c |= *p << *i;
         *p >>= 6 - *i;
         *i += BITS-8;
      }
      else {
         *p = (word)tail(*q),  *q = val(*q);
         c |= *p << *i;
         *p >>= 6 - *i;
         *i += BITS-6;
      }
      c &= 63;
   }
   return Ascii7[c];
}

any mkTxt(int c) {return txt(Ascii6[c & 127]);}

any mkChar(int c) {
   return consSym(NULL, Ascii6[c & 127]);
}

any mkChar2(int c, int d) {
   c = Ascii6[c & 127];
   d = Ascii6[d & 127];
   return consSym(NULL, d << (c & 1? 7 : 6) | c);
}

void putByte0(int *i, word *p, any *q) {
   *i = 0,  *p = 0,  *q = NULL;
}

void putByte1(int c, int *i, word *p, any *q) {
   *i = (*p = Ascii6[c & 127]) & 1? 7 : 6;
   *q = NULL;
}

void putByte(int c, int *i, word *p, any *q, cell *cp) {
   int d = (c = Ascii6[c & 127]) & 1? 7 : 6;

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

any intern(any sym, any tree[2]) {
   any nm, x, y, z;
   long n;

   if ((nm = name(sym)) == txt(0))
      return sym;
   if (isTxt(nm)) {
      if (!isCell(x = tree[0])) {
         tree[0] = cons(sym, Nil);
         return sym;
      }
      for (;;) {
         if ((n = (word)nm - (word)name(car(x))) == 0)
            return car(x);
         if (!isCell(cdr(x))) {
            cdr(x) = n<0? cons(cons(sym,Nil), Nil) : cons(Nil, cons(sym,Nil));
            return sym;
         }
         if (n < 0) {
            if (isCell(cadr(x)))
               x = cadr(x);
            else {
               cadr(x) = cons(sym, Nil);
               return sym;
            }
         }
         else {
            if (isCell(cddr(x)))
               x = cddr(x);
            else {
               cddr(x) = cons(sym, Nil);
               return sym;
            }
         }
      }
   }
   else {
      if (!isCell(x = tree[1])) {
         tree[1] = cons(sym, Nil);
         return sym;
      }
      for (;;) {
         y = nm,  z = name(car(x));
         while ((n = (word)tail(y) - (word)tail(z)) == 0) {
            y = val(y),  z = val(z);
            if (isNum(y)) {
               if (y == z)
                  return car(x);
               n = isNum(z)? y-z : -1;
               break;
            }
            if (isNum(z)) {
               n = +1;
               break;
            }
         }
         if (!isCell(cdr(x))) {
            cdr(x) = n<0? cons(cons(sym,Nil), Nil) : cons(Nil, cons(sym,Nil));
            return sym;
         }
         if (n < 0) {
            if (isCell(cadr(x)))
               x = cadr(x);
            else {
               cadr(x) = cons(sym, Nil);
               return sym;
            }
         }
         else {
            if (isCell(cddr(x)))
               x = cddr(x);
            else {
               cddr(x) = cons(sym, Nil);
               return sym;
            }
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

bool isBlank(any x) {
   int i, c;
   word w;

   if (!isSymb(x))
      return NO;
   if (isNil(x))
      return YES;
   x = name(x);
   for (c = getByte1(&i, &w, &x);  c;  c = getByte(&i, &w, &x))
      if (c > ' ')
         return NO;
   return YES;
}


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

void pack(any x, int *i, word *p, any *q, cell *cp) {
   int c, j;
   word w;

   if (isCell(x))
      do
         pack(car(x), i, p, q, cp);
      while (isCell(x = cdr(x)));
   if (isNum(x)) {
      char buf[BITS/2], *b = buf;

      bufNum(buf, unBox(x));
      do
         putByte(*b++, i, p, q, cp);
      while (*b);
   }
   else if (!isNil(x))
      for (x = name(x), c = getByte1(&j, &w, &x); c; c = getByte(&j, &w, &x))
         putByte(c, i, p, q, cp);
}

///////////////////////////////////////////////
//               sym.c - END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               io.c - START
///////////////////////////////////////////////

static any read0(bool);

static int StrI;
static cell StrCell, *StrP;
static word StrW;
static void (*PutSave)(int);
static char Delim[] = " \t\n\r\"'(),[]`~{}";

static void openErr(any ex, char *s) {err(ex, NULL, "%s open: %s", s, strerror(errno));}
static void eofErr(void) {err(NULL, NULL, "EOF Overrun");}

/* Buffer size */
int bufSize(any x) {return symBytes(x) + 1;}

int pathSize(any x) {
   int c = firstByte(x);

   if (c != '@'  &&  (c != '+' || secondByte(x) != '@'))
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

void wrOpen(any ex, any x, outFrame *f) {
   NeedSymb(ex,x);
   if (isNil(x))
      f->fp = stdout;
   else {
      char nm[pathSize(x)];

      pathString(x,nm);
      if (nm[0] == '+') {
         if (!(f->fp = fopen(nm+1, "a")))
            openErr(ex, nm);
      }
      else if (!(f->fp = fopen(nm, "w")))
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

static void putString(int c) {
   putByte(c, &StrI, &StrW, &StrP, &StrCell);
}

void begString(void) {
   putByte0(&StrI, &StrW, &StrP);
   PutSave = Env.put,  Env.put = putString;
}

any endString(void) {
   Env.put = PutSave;
   StrP = popSym(StrI, StrW, StrP, &StrCell);
   return StrI? StrP : Nil;
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

///////////////////////////////////////////////
//               io.c - END
///////////////////////////////////////////////


///////////////////////////////////////////////
//               math.h
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

/* Make symbol from number */
any numToSym(any x, int scl, int sep, int ign) {
   int i;
   word w;
   cell c1;
   long n;
   byte *p, buf[BITS/2];

   n = unBox(x);
   putByte0(&i, &w, &x);
   if (n < 0) {
      n = -n;
      putByte('-', &i, &w, &x, &c1);
   }
   for (p = buf;;) {
      *p = n % 10;
      if ((n /= 10) == 0)
         break;
      ++p;
   }
   if ((scl = p - buf - scl) < 0) {
      putByte('0', &i, &w, &x, &c1);
      putByte(sep, &i, &w, &x, &c1);
      while (scl < -1)
         putByte('0', &i, &w, &x, &c1),  ++scl;
   }
   for (;;) {
      putByte(*p + '0', &i, &w, &x, &c1);
      if (--p < buf)
         return popSym(i, w, x, &c1);
      if (scl == 0)
         putByte(sep, &i, &w, &x, &c1);
      else if (ign  &&  scl > 0  &&  scl % 3 == 0)
         putByte(ign, &i, &w, &x, &c1);
      --scl;
   }
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

// (abs 'num) -> num
any doAbs(any ex) {
   any x;

   x = cdr(ex);
   if (isNil(x = EVAL(car(x))))
      return Nil;
   NeedNum(ex,x);
   return num(x)<0? box(-unBox(x)) : x;
}


///////////////////////////////////////////////
//               math.h END
///////////////////////////////////////////////

/*** System ***/
void giveup(char *msg) {
   fprintf(stderr, "%s\n", msg);
   exit(1);
}

void bye(int n) {
   static bool b;

   if (!b) {
      b = YES;
      unwind(NULL);
      prog(val(Bye));
   }
   exit(n);
}

void execError(char *s) {
   fprintf(stderr, "%s: Can't exec\n", s);
   exit(127);
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

long compare(any x, any y) {
   any a, b;

   if (x == y)
      return 0;
   if (isNil(x))
      return -1;
   if (x == T)
      return +1;
   if (isNum(x)) {
      if (!isNum(y))
         return isNil(y)? +1 : -1;
      return num(x) - num(y);
   }
   if (isSym(x)) {
      int c, d, i, j;
      word w, v;

      if (isNum(y) || isNil(y))
         return +1;
      if (isCell(y) || y == T)
         return -1;
      a = name(x),  b = name(y);
      if (a == txt(0) && b == txt(0))
         return (long)x - (long)y;
      if ((c = getByte1(&i, &w, &a)) == (d = getByte1(&j, &v, &b)))
         do
            if (c == 0)
               return 0;
         while ((c = getByte(&i, &w, &a)) == (d = getByte(&j, &v, &b)));
      return c - d;
   }
   if (!isCell(y))
      return y == T? -1 : +1;
   a = x, b = y;
   for (;;) {
      long n;

      if (n = compare(car(x),car(y)))
         return n;
      if (!isCell(x = cdr(x)))
         return compare(x, cdr(y));
      if (!isCell(y = cdr(y)))
         return y == T? -1 : +1;
      if (x == a && y == b)
         return 0;
   }
}

/*** Error handling ***/
void err(any ex, any x, char *fmt, ...) {
   va_list ap;
   char msg[240];
   outFrame f;

   Chr = 0;
   Env.brk = NO;
   f.fp = stderr;
   pushOutFiles(&f);
   while (*AV  &&  strcmp(*AV,"-") != 0)
      ++AV;
   if (ex)
      outString("!? "), print(val(Up) = ex), newline();
   if (x)
      print(x), outString(" -- ");
   va_start(ap,fmt);
   vsnprintf(msg, sizeof(msg), fmt, ap);
   va_end(ap);
   if (msg[0]) {
      outString(msg), newline();
      val(Msg) = mkStr(msg);
      if (!isNil(val(Err)) && !Jam)
         Jam = YES,  prog(val(Err)),  Jam = NO;
      load(NULL, '?', Nil);
   }
   unwind(NULL);
   Env.stack = NULL;
   Env.next = -1;
   Env.make = Env.yoke = NULL;
   Env.parser = NULL;
   Trace = 0;
   Env.put = putStdout;
   Env.get = getStdin;
   longjmp(ErrRst, +1);
}


void argError(any ex, any x) {err(ex, x, "Bad argument");}
void numError(any ex, any x) {err(ex, x, "Number expected");}
void symError(any ex, any x) {err(ex, x, "Symbol expected");}
void pairError(any ex, any x) {err(ex, x, "Cons pair expected");}
void atomError(any ex, any x) {err(ex, x, "Atom expected");}
void lstError(any ex, any x) {err(ex, x, "List expected");}
void varError(any ex, any x) {err(ex, x, "Variable expected");}
void protError(any ex, any x) {err(ex, x, "Protected symbol");}

void unwind(catchFrame *catch) {
   any x;
   int i, j, n;
   bindFrame *p;
   catchFrame *q;

   while (q = CatchPtr) {
      while (p = Env.bind) {
         if ((i = p->i) < 0) {
            j = i, n = 0;
            while (++n, ++j && (p = p->link))
               if (p->i >= 0 || p->i < i)
                  --j;
            do {
               for (p = Env.bind, j = n;  --j;  p = p->link);
               if (p->i < 0  &&  ((p->i -= i) > 0? (p->i = 0) : p->i) == 0)
                  for (j = p->cnt;  --j >= 0;) {
                     x = val(p->bnd[j].sym);
                     val(p->bnd[j].sym) = p->bnd[j].val;
                     p->bnd[j].val = x;
                  }
            } while (--n);
         }
         if (Env.bind == q->env.bind)
            break;
         if (Env.bind->i == 0)
            for (i = Env.bind->cnt;  --i >= 0;)
               val(Env.bind->bnd[i].sym) = Env.bind->bnd[i].val;
         Env.bind = Env.bind->link;
      }
      while (Env.inFrames != q->env.inFrames)
         popInFiles();
      while (Env.outFrames != q->env.outFrames)
         popOutFiles();
      Env = q->env;
      EVAL(q->fin);
      CatchPtr = q->link;
      if (q == catch)
         return;
   }
   while (Env.bind) {
      if (Env.bind->i == 0)
         for (i = Env.bind->cnt;  --i >= 0;)
            val(Env.bind->bnd[i].sym) = Env.bind->bnd[i].val;
      Env.bind = Env.bind->link;
   }
   while (Env.inFrames)
      popInFiles();
   while (Env.outFrames)
      popOutFiles();
}

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

/* Evaluate number */
long evNum(any ex, any x) {return xNum(ex, EVAL(car(x)));}

long xNum(any ex, any x) {
   NeedNum(ex,x);
   return unBox(x);
}

/* Evaluate any to sym */
any evSym(any x) {return xSym(EVAL(car(x)));}

any xSym(any x) {
   int i;
   word w;
   any y;
   cell c1, c2;

   if (isSymb(x))
      return x;
   Push(c1,x);
   putByte0(&i, &w, &y);
   i = 0,  pack(x, &i, &w, &y, &c2);
   y = popSym(i, w, y, &c2);
   drop(c1);
   return i? y : Nil;
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
