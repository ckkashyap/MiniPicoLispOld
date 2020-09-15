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

typedef int INT;
typedef enum {NO,YES} BOOL;
INT BITS = (INT)sizeof(char*) * 8;
INT Chr, MemIdx;
static char Delim[] = " \t\n\r\"'(),[]`~{}";
static char Token[1024];
INT CELL_SIZE_BYTES = 3 * sizeof(WORD_TYPE);


typedef struct symbol
{
   char *nm;
   INT val;
   struct symbol *less, *more;
} symbol;

static symbol *Intern, *Transient;

#define Nil     (0)
#define T       (1)
#define Quote   (2)

static char **Mem;

typedef enum
{
    Undefined,
    Number,
    Symbol,
    Pair,
    Bin,
    Txt,
    External,
} Type;

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
    sprintf(buf, WORD_FORMAT_STRING, w);
    addMem(buf);
}

void addType(Type type)
{
    char buf[100];
    sprintf(buf, "%d", type);
    addMem(buf);
}

void addNextPtr()
{
    char buf[100];
    sprintf(buf, "(Mem + %d + 2)", MemIdx);
    addMem(buf);  
}

static void mkSym(char *name, char *value)
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
                addNextPtr();
                addWord(0); // TODO -> this should really be value
                addType(Undefined); // TODO - find the right type 
            }

            addWord(w);
            addNextPtr();
            addType(Undefined); // TODO - find the right type

            w = c >> BITS - i;
            i -= BITS;
        }

        i += d;
    }

    addWord(w);
    addWord(0); // TODO - what should this be?
    addType(Undefined); // TODO - find the right type
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
        insert(&Transient, Token, x = ramSym(Token, buf, Symbol));
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

    insert(&Intern, Token, x = ramSym(Token, "(Ram+1)", Symbol));
    return x;
} 

INT main(INT ac, char *av[])
{

    mkSym("aaaaaaaaaa", "10");

    for (INT i = 0; i < MemIdx; i += 3)
    {
        printf("%s, %s, %s\n", Mem[i], Mem[i + 1], Mem[i + 2]);
    }

    return 0;
}
