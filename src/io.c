/* 16apr18abu
 * (c) Software Lab. Alexander Burger
 */

#include "pico.h"

any doHide(any);
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
