#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worddefinition.h"

#define Nil     (0)
#define T       (1)
#define Quote   (2)

INT CELL_SIZE_BYTES = 3 * sizeof(WORD_TYPE);
INT BITS = (INT)sizeof(char*) * 8;

INT Chr, MemIdx;
static char Delim[] = " \t\n\r\"'(),[]`~{}";
static char Token[1024];

typedef struct symbol
{
   char *nm;
   INT val;
   struct symbol *less, *more;
} symbol;

static symbol *Intern;

static char **Mem;
static INT *Types;

static void giveup(char *msg)
{
   fprintf(stderr, "gen: %s\n", msg);
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


void addMem(char *v)
{
    Mem = realloc(Mem, (MemIdx + 1) * sizeof(char*));
    Mem[MemIdx++] = strdup(v);
}

void addWord(UNSIGNED_WORD_TYPE w)
{
    char buf[100];
    sprintf(buf, "(" WORD_FORMAT_STRING ")", w);
    addMem(buf);
}

WORD mkType(Type t1, Type t2)
{
    WORD t=0;
    char *p = (char*)&t;
    p[1] = (t1 << 4) | t2;
    return t;
}

void addType(Type type)
{
    char buf[100];
    sprintf(buf, WORD_FORMAT_STRING, (WORD)type);
    addMem(buf);

    Types = realloc(Types, (MemIdx / 3) * sizeof(INT));
    Types[(MemIdx/3)-1] = type;
}

void addSym(INT x)
{
    char buf[100];
    sprintf(buf, "(Any)(Mem + %d)", x);
    addMem(buf);  
}

void addNextPtr(INT o)
{
    char buf[100];
    sprintf(buf, "(Any)(Mem + %d + %d)", MemIdx, o);
    addMem(buf);  
}

static void mkSym(char *name, char *value, Type type)
{
    BOOL BIN = NO;
    INT i, c, d;
    UNSIGNED_WORD_TYPE w;
    i = 7;

    w = *name++;
    while(*name)
    {
        c = *name++;
        d = 7;

        if (i != BITS)
        {
            w |= (UNSIGNED_WORD_TYPE)c << i;
        }

        if ((i + d) > BITS)
        {
            if (BIN == NO)
            {
                BIN = YES;
                addNextPtr(3);
                addMem(value);
                addType(mkType(Type_Bin_Start, type));
            }

            addWord(w);
            addNextPtr(2);
            addType(mkType(BIN==YES? Type_Bin : Type_Txt, Type_Pair));

            w = c >> BITS - i;
            i -= BITS;
        }

        i += d;
    }

    addWord(w);
    if (BIN)
    {
        addWord(0); // TODO - 0 is the end of string
        addType(mkType(Type_Bin_End, Type_Num));
    }
    else
    {
        addMem(value);
        addType(mkType(Type_Txt, type));
    }
}

static INT skip(void)
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

static int ramSym(char *name, char *value, Type type)
{
   INT ix = MemIdx;

   mkSym(name, value, type);
   return ix;
}


static void insert(symbol **tree, char *name, INT value)
{
   symbol *p, **t;

   p = malloc(sizeof(symbol));
   p->nm = strdup(name);
   p->val = value;
   p->less = p->more = NULL;
   for (t = tree;  *t;  t = strcmp(name, (*t)->nm) >= 0? &(*t)->more : &(*t)->less);
   *t = p;
}

static INT lookup(symbol **tree, char *name)
{
   symbol *p;
   INT n;

   for (p = *tree;  p;  p = n > 0? p->more : p->less)
   {
      if ((n = strcmp(name, p->nm)) == 0)
      {
         return p->val;
      }
   }
   return 0;
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
   int y = rdList(z ? z : x);
   return cons(x, y);
}

static int cons(int x, int y)
{
   int i, ix = MemIdx;
   char car[40], cdr[40];

   addSym(x);
   addSym(y);
   addType(mkType(Type_Sym, Type_Sym));

   return ix;
}

static INT read0(BOOL top)
{
    INT x;
    WORD_TYPE w;
    char *p, buf[100];

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

        if (x = lookup(&Intern, Token))
        {
            return x;
        }

        sprintf(buf,"(Mem+%d)", MemIdx);
        insert(&Intern, Token, x = ramSym(Token, buf, Type_Pair));
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
        x = MemIdx;
        addWord(w);
        addWord(0);
        addType(mkType(Type_Num, Type_Undefined));
        return x;
    }

    if (x = lookup(&Intern, Token))
    {
        return x;
    }

    insert(&Intern, Token, x = ramSym(Token, "0/*Undefined1*/", Type_Sym));
    return x;
} 

/* Test for escaped characters */
static BOOL testEsc(void)
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

INT main(INT ac, char *av[])
{
    char buf[100];
    char *p;
    INT x; 
    FILE *fpSYM;
    FILE *fpMem = fopen("mem.h", "w");

    if ((fpSYM = fopen("sym.h", "w")) == NULL)
    {
        giveup("Can't create output files");
    }

    fprintf(fpSYM, "#define Nil (Mem+0)\n");

    ac--;

    ramSym("Nil", "(Mem)", Type_Sym);

    do
    {
        char *n = *++av;
        printf("Loading file %s\n", n);
        if (!freopen(n, "r", stdin))
        {
            giveup("Can't open input file");
        }

        Chr = getchar();
        while ((x = read0(YES)) != Nil)
        {
            if (skip() == '[')
            {                   // C Identifier
                fprintf(fpSYM, "#define ");
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

                    putc(Chr, fpSYM);
                }

                //print(buf, x);
                fprintf(fpSYM, " (any)%s\n", buf);
            }

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
                sprintf(buf, "((Any)(%s))", Token);
                Mem[x] = strdup(buf);
                fprintf(fpSYM, "Any %s(Any);\n", Token);
            }
            else
            {                                 // Value
                int v = read0(YES);
                sprintf(buf, "(Mem+%d)", v);
                Mem[x + 1] = strdup(buf);
                //sprintf(buf, WORD_FORMAT_STRING, );
                //Mem[x + 2] = Types[x];
            }

            while (skip() == ',')          // Properties
            {
                Chr = getchar();
                if (Chr == EOF)
                {
                    break;
                }

                //print(buf, read0(YES));


                //print(buf, (RamIx-4) << 2);
                //Ram[x-1] = strdup(buf);
                //sprintf(buf, "%d", Symbol);
                //Ram[x+1] = strdup(buf);
            }
        }
    }
    while (--ac);

    // x = ramSym("abc", "10", Type_Num);
    // printf("%d\n", x);
    // x = ramSym("abcdefghij", "10", Type_Num);
    // printf("%d\n", x);
    // x = ramSym("abcdefghij", "20", Type_Num);
    // printf("%d\n", x);
    // x = ramSym("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl", "20", Type_Num);
    // printf("%d\n", x);

    fprintf(fpMem, "#define MEMS %d\n", MemIdx);
    fprintf(fpMem, "Any Mem[] = {\n");
    for (INT i = 0; i < MemIdx; i += 3)
    {
        fprintf(fpMem, "    (Any)%s, (Any)%s, (Any)%s,\n", Mem[i], Mem[i + 1], Mem[i + 2]);
    }
    fprintf(fpMem, "};\n");
    fclose(fpMem);



    return 0;
}
