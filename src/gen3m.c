/* 29nov17abu
 * (c) Software Lab. Alexander Burger
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if INTPTR_MAX == INT32_MAX
    #define WORD_TYPE int32_t
    #define UNSIGNED_WORD_TYPE uint32_t
    #define WORD_FORMAT_STRING "0x%lx"
#elif INTPTR_MAX == INT64_MAX
    #define WORD_TYPE int64_t
    #define UNSIGNED_WORD_TYPE uint64_t
    #define WORD_FORMAT_STRING "0x%llx"
#else
    #error "Unsupported bit width"
#endif

typedef UNSIGNED_WORD_TYPE word;
typedef unsigned char byte;

#undef bool
typedef enum {NO,YES} bool;

#define txt(n)    (n << 1| 1)
#define box(n)    (n << 2| 2)

#define Nil    (1 << 2)
#define T      (5 << 2)
#define Quote  (7 << 2)

static int Bits, Chr, RomIx, RamIx;
static char **Rom, **Ram;
static char Token[1024];

static int read0(bool);
static char Delim[] = " \t\n\r\"'(),[]`~{}";

typedef struct symbol
{
   char *nm;
   int val;
   struct symbol *less, *more;
} symbol;

static symbol *Intern, *Transient;

static byte Ascii6[] =
{
   0,  2,  2,  2,  2,  2,  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,  2,  2,  2,  2,  2,  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
   2,  1,  3,  5,  7,  9, 11,  13,  15,  17,  19,  21,  23,  25,   4,   6,
  27, 29, 31, 33, 35, 37, 39,  41,  43,  45,  47,  49,   8,  51,  10,  53,
  55, 57, 59, 61, 63, 65, 67,  69,  71,  73,  75,  77,  79,  81,  83,  85,
  87, 89, 91, 93, 95, 97, 99, 101, 103, 105, 107, 109, 111, 113, 115, 117,
 119, 12, 14, 16, 18, 20, 22,  24,  26,  28,  30,  32,  34,  36,  38,  40,
  42, 44, 46, 48, 50, 52, 54,  56,  58,  60,  62, 121, 123, 125, 127,   0
};

static void giveup(char *msg)
{
   fprintf(stderr, "gen3m: %s\n", msg);
   exit(1);
}

static void noReadMacros(void)
{
   giveup("Can't support read-macros");
}

static void eofErr(void)
{
   giveup("EOF Overrun");
}

static void addList(int *ix, char ***list, char *fmt, WORD_TYPE x)
{
    char buf[40];

    *list = realloc(*list, (*ix + 1) * sizeof(char*));
    if (x)
    {
        sprintf(buf, fmt, x);
        (*list)[(*ix)++] = strdup(buf);
    }
    else
    {
        (*list)[(*ix)++] = strdup(fmt);
    }
}

static void mkRamSym(int *ix, char ***list, char *mem, char *name, char *value)
{
   bool bin;
   int i, c, d;
   word w;

   bin = NO;
   i = (w = Ascii6[*name++ & 127]) & 1? 7 : 6;
   while (*name)
   {
      d = (c = Ascii6[*name++ & 127]) & 1? 7 : 6;

      if (i != Bits)
      {
         w |= (word)c << i;
      }
      
      if (i + d  > Bits)
      {
         if (bin)
         {
            addList(&RamIx, &Ram, "(Ram+%d)", RamIx + 2);
         }
         else
         {
            addList(ix, list, "(Ram+%d)", RamIx + 3);
            addList(ix, list, value, 0);
            bin = YES;
         }

         addList(&RamIx, &Ram, WORD_FORMAT_STRING, w);
         w = c >> Bits - i;
         i -= Bits;
      }

      i += d;
   }

   if (bin)
   {
      if (i <= (Bits-2))
      {
         addList(&RamIx, &Ram, WORD_FORMAT_STRING, box(w));
      }
      else
      {
         addList(&RamIx, &Ram, "(Ram+%d)", RamIx + 2);
         addList(&RamIx, &Ram, WORD_FORMAT_STRING, w);
         addList(&RamIx, &Ram, "2", 0);
      }
   }
   else if (i > Bits-1)
   {
      addList(ix, list, "(Ram+%d)", RamIx + 3);
      addList(ix, list, value, 0);
      addList(&RamIx, &Ram, WORD_FORMAT_STRING, w);
      addList(&RamIx, &Ram, "2", 0);
   }
   else
   {
      addList(ix, list, WORD_FORMAT_STRING, txt(w));
      addList(ix, list, value, 0);
   }
}

static void mkSym(int *ix, char ***list, char *mem, char *name, char *value)
{
   bool bin;
   int i, c, d;
   word w;

   bin = NO;
   i = (w = Ascii6[*name++ & 127]) & 1? 7 : 6;
   while (*name)
   {
      d = (c = Ascii6[*name++ & 127]) & 1? 7 : 6;

      if (i != Bits)
      {
         w |= (word)c << i;
      }
      
      if (i + d  > Bits)
      {
         if (bin)
         {
            addList(&RomIx, &Rom, "(Rom+%d)", RomIx + 2);
         }
         else
         {
            addList(ix, list, "(Rom+%d)", RomIx + (ix == &RomIx? 3 : 1));
            addList(ix, list, value, 0);
            bin = YES;
         }

         addList(&RomIx, &Rom, WORD_FORMAT_STRING, w);
         w = c >> Bits - i;
         i -= Bits;
      }

      i += d;
   }

   if (bin)
   {
      if (i <= (Bits-2))
      {
         addList(&RomIx, &Rom, WORD_FORMAT_STRING, box(w));
      }
      else
      {
         addList(&RomIx, &Rom, "(Rom+%d)", RomIx + 2);
         addList(&RomIx, &Rom, WORD_FORMAT_STRING, w);
         addList(&RomIx, &Rom, "2", 0);
      }
   }
   else if (i > Bits-1)
   {
      addList(ix, list, "(Rom+%d)", RomIx + (ix == &RomIx? 3 : 1));
      addList(ix, list, value, 0);
      addList(&RomIx, &Rom, WORD_FORMAT_STRING, w);
      addList(&RomIx, &Rom, "2", 0);
   }
   else
   {
      addList(ix, list, WORD_FORMAT_STRING, txt(w));
      addList(ix, list, value, 0);
   }
}

static void print(char buf[], int x)
{
   if (x & 2)
   {
      sprintf(buf, "%d", x);
   }
   else if ((x >>= 2) > 0)
   {
      sprintf(buf, "(Rom+%d)", x);
   }
   else
   {
      sprintf(buf, "(Ram+%d)", -x);
   }
}

static int consRam(int x, int y)
{
   int i, ix = RomIx;
   char car[40], cdr[40];

   print(car, -x);
   print(cdr, -y);
   for (i = 0; i < RamIx;  i += 2)
   {
      if (strcmp(car, Ram[i]) == 0  &&  strcmp(cdr, Ram[i+1]) == 0)
      {
         return -i << 2;
      }
   }
   addList(&RamIx, &Ram, car, 0);
   addList(&RamIx, &Ram, cdr, 0);
   return -ix << 2;
}

static int cons(int x, int y)
{
   int i, ix = RomIx;
   char car[40], cdr[40];

   print(car, x);
   print(cdr, y);
   for (i = 0; i < RomIx;  i += 2)
   {
      if (strcmp(car, Rom[i]) == 0  &&  strcmp(cdr, Rom[i+1]) == 0)
      {
         return i << 2;
      }
   }
   addList(&RomIx, &Rom, car, 0);
   addList(&RomIx, &Rom, cdr, 0);
   return ix << 2;
}

static int romSym(char *name, char *value)
{
   int ix = RomIx;

   mkSym(&RomIx, &Rom, "(Rom+%d)", name, value);
   return ix + 1 << 2;
}

static int ramSym(char *name, char *value)
{
   int ix = RamIx;

   mkRamSym(&RamIx, &Ram, "(Ram+%d)", name, value);
   return -(ix + 1) << 2;
}

static void insert(symbol **tree, char *name, int value)
{
   symbol *p, **t;

   p = malloc(sizeof(symbol));
   p->nm = strdup(name);
   p->val = value;
   p->less = p->more = NULL;
   for (t = tree;  *t;  t = strcmp(name, (*t)->nm) >= 0? &(*t)->more : &(*t)->less);
   *t = p;
}

static int lookup(symbol **tree, char *name)
{
   symbol *p;
   int n;

   for (p = *tree;  p;  p = n > 0? p->more : p->less)
   {
      if ((n = strcmp(name, p->nm)) == 0)
      {
         return p->val;
      }
   }
   return 0;
}

static int skip(void)
{
   for (;;)
   {
      if (Chr < 0)
      {
         return Chr;
      }
      while (Chr <= ' ')
      {
         Chr = getchar();
         if (Chr < 0)
         {
            return Chr;
         }
      }

      if (Chr != '#')
      {
         return Chr;
      }
      Chr = getchar();
      if (Chr != '{')
      {
         while (Chr != '\n')
         {
            if (Chr < 0)
            {
               return Chr;
            }
            Chr = getchar();
         }
      }
      else
      {
         for (;;)
         {
            Chr = getchar();
            if (Chr < 0)
            {
               return Chr;
            }
            if (Chr == '}' && (Chr = getchar(), Chr == '#'))
            {
               break;
            }
         }
         Chr = getchar();
      }
   }
}

/* Test for escaped characters */
static bool testEsc(void)
{
   for (;;)
   {
      if (Chr < 0)
      {
         return NO;
      }

      if (Chr != '\\')
      {
         return YES;
      }

      if (Chr = getchar(), Chr != '\n')
      {
         return YES;
      }

      do
      {
         Chr = getchar();
      }
      while (Chr == ' '  ||  Chr == '\t');
   }
}

/* Read a list */
static int rdList(int z)
{
   int x;

   if (skip() == ')')
   {
      Chr = getchar();
      return Nil;
   }

   if (Chr == ']')
   {
      return Nil;
   }

   if (Chr == '~')
   {
      noReadMacros();
   }

   if (Chr == '.')
   {
      Chr = getchar();
      x = skip()==')' || Chr==']'? z : read0(NO);
      if (skip() == ')')
      {
         Chr = getchar();
      }
      else if (Chr != ']')
      {
         giveup("Bad dotted pair");
      }

      return x;
   }

   x = read0(NO);
   return cons(x, rdList(z ? z : x));
}

/* Read one expression */
static int read0(bool top)
{
   int x;
   word w;
   char *p, buf[40];

   if (skip() < 0)
   {
      if (top)
         return Nil;
      eofErr();
   }

   if (Chr == '(')
   {
      Chr = getchar();
      x = rdList(0);
      if (top  &&  Chr == ']')
      {
         Chr = getchar();
      }
      return x;
   }

   if (Chr == '[')
   {
      Chr = getchar();
      x = rdList(0);
      if (Chr != ']')
      {
         giveup("Super parentheses mismatch");
      }
      Chr = getchar();
      return x;
   }

   if (Chr == '\'')
   {
      Chr = getchar();
      return cons(Quote, read0(top));
   }

   if (Chr == '`')
   {
      noReadMacros();
   }

   if (Chr == '"')
   {
      Chr = getchar();
      if (Chr == '"')
      {
         Chr = getchar();
         return Nil;
      }

      for (p = Token;;)
      {
         if (!testEsc())
         {
            eofErr();
         }
         *p++ = Chr;
         if (p == Token+1024)
         {
            giveup("Token too long");
         }
         
         if ((Chr = getchar()) == '"')
         {
            Chr = getchar();
            break;
         }
      }

      *p = '\0';
      if (x = lookup(&Transient, Token))
      {
         return x;
      }

      print(buf, -(RamIx + 1) << 2);
      insert(&Transient, Token, x = ramSym(Token, buf));
      return x;
   }

   if (strchr(Delim, Chr))
   {
      giveup("Bad input");
   }

   if (Chr == '\\')
   {
      Chr = getchar();
   }

   for (p = Token;;)
   {
      *p++ = Chr;
      if (p == Token+1024)
      {
         giveup("Token too long");
      }

      Chr = getchar();
      if (strchr(Delim, Chr))
      {
         break;
      }

      if (Chr == '\\')
      {
         Chr = getchar();
      }
   }

   *p = '\0';
   w = strtol(Token, &p, 10);
   if (p != Token && *p == '\0')
   {
      return box(w);
   }

   if (x = lookup(&Intern, Token))
   {
      return x;
   }

   insert(&Intern, Token, x = ramSym(Token, "(Rom+1)"));
   return x;
}

int main(int ac, char *av[])
{
   int x, ix;
   FILE *fp;
   char *p, buf[40];

   if ((ac -= 2) <= 0)
   {
      giveup("No input files");
   }

   if ((Bits = atoi(*++av)) == 0)
   {
      Bits = (int)sizeof(char*) * 8;
   }

   if ((fp = fopen("sym.d", "w")) == NULL)
   {
      giveup("Can't create output files");
   }

   insert(&Intern, "NIL", ramSym("NIL", "(Ram+1)"));
   consRam(Nil, Nil);
   fprintf(fp, "#define Nil (any)(Ram+1)\n");
   insert(&Intern, "T", ramSym("T", "(Ram+5)"));
   fprintf(fp, "#define T (any)(Ram+5)\n");
   insert(&Intern, "quote", ramSym("quote", "(num(doQuote) + 2)"));
   fprintf(fp, "#define Quote (any)(Ram+7)\nany doQuote(any);\n");
   do
   {
      if (!freopen(*++av, "r", stdin))
      {
         giveup("Can't open input file");
      }

      Chr = getchar();
      while ((x = read0(YES)) != Nil)
      {
         if (x & 2  ||  (x & 4) == 0)
         {
            giveup("Symbol expected");
         }

         if (skip() == '[')
         {                   // C Identifier
            fprintf(fp, "#define ");
            for (;;)
            {
               Chr = getchar();
               if (Chr == EOF)
               {
                  break;
               }

               if (Chr == ']')
               {
                  Chr = getchar();
                  break;
               }

               putc(Chr, fp);
            }

            print(buf, x);
            fprintf(fp, " (any)%s\n", buf);
         }

         x >>= 2;
         if (skip() == '{')
         {                   // Function pointer
            for (p = Token;;)
            {
               Chr = getchar();
               if (Chr == EOF)
               {
                  break;
               }

               if (Chr == '}')
               {
                  Chr = getchar();
                  break;
               }

               *p++ = Chr;
            }

            *p = '\0';
            sprintf(buf, "(num(%s) + 2)", Token);
            Ram[-x] = strdup(buf);
            fprintf(fp, "any %s(any);\n", Token);
         }
         else
         {                                 // Value
            print(buf, read0(YES));
            if (x > 0)
            {
               Rom[x] = strdup(buf);
            }
            else
            {
               Ram[-x] = strdup(buf);
            }
         }

         while (skip() == ',')          // Properties
         {
            Chr = getchar();
            if (Chr == EOF)
            {
               break;
            }

            print(buf, read0(YES));
            ix = RomIx;
            if (x > 0)
            {
               addList(&RomIx, &Rom, Rom[x-1], 0);
               addList(&RomIx, &Rom, buf, 0);
               print(buf, ix << 2);
               Rom[x-1] = strdup(buf);
            }
            else
            {
               addList(&RomIx, &Rom, Ram[-x-1], 0);
               addList(&RomIx, &Rom, buf, 0);
               print(buf, ix << 2);
               Ram[-x-1] = strdup(buf);
            }
         }
      }
   }
   while (--ac);

   fprintf(fp, "\n#define ROMS %d\n", RomIx);
   fprintf(fp, "#define RAMS %d\n", RamIx);
   fclose(fp);
   if (fp = fopen("rom.d", "w"))
   {
      for (x = 0; x < RomIx; x += 2)
      {
         fprintf(fp, "(any)%s, (any)%s,\n", Rom[x], Rom[x+1]);
      }
      fclose(fp);
   }

   if (fp = fopen("ram.d", "w"))
   {
      for (x = 0; x < RamIx; x += 2)
      {
         fprintf(fp, "(any)%s, (any)%s,\n", Ram[x], Ram[x+1]);
      }

      fclose(fp);
   }

   return 0;
}
