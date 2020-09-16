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

static symbol *Intern, *Transient;

static char **Mem;

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

void addType(Type type)
{
    char buf[100];
    sprintf(buf, "%d", type);
    addMem(buf);
}

void addNextPtr(INT o)
{
    char buf[100];
    sprintf(buf, "(Any)(Mem + %d + %d)", MemIdx, o);
    addMem(buf);  
}

WORD mkType(Type t1, Type t2)
{
    WORD t;
    char *p = (char*)&t;
    p[0] = t1;
    p[1] = t2;
    return t;
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

static void mkNum(char *name, WORD value)
{
    char buf[100];
    sprintf(buf, WORD_FORMAT_STRING, value);
    mkSym(name, buf, Type_Num);
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

        if (x = lookup(&Transient, Token))
        {
            return x;
        }

        sprintf(buf,"(Mem+%d)", MemIdx);
        //insert(&Transient, Token, x = ramSym(Token, buf, Symbol));
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

    //insert(&Intern, Token, x = ramSym(Token, "(Ram+1)", Symbol));
    return x;
} 

INT main(INT ac, char *av[])
{
    FILE *mem = fopen("mem.h", "w");

    mkSym("abcdefghij", "10", Type_Num);
    mkSym("abcdefghij", "20", Type_Num);
    mkSym("abcdefghijklmnopqrstuvwxyz", "20", Type_Num);

    fprintf(mem, "#define MEMS %d\n", MemIdx);
    fprintf(mem, "Any Mem[] = {\n");
    for (INT i = 0; i < MemIdx; i += 3)
    {
        fprintf(mem, "    (Any)%s, (Any)%s, (Any)%s,\n", Mem[i], Mem[i + 1], Mem[i + 2]);
    }
    fprintf(mem, "};\n");
    fclose(mem);

    return 0;
}
