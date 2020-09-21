#include "worddefinition.h"

INT BITS = (INT)sizeof(char*) * 8;

#include "sym.h"
#include "mem.h"

Type gtType1(WORD w)
{
    char *p = (char*)&w;
    int t = p[1];
    return t >> 4;
}

Type gtType2(WORD w)
{
    char *p = (char*)&w;
    int t = p[1];
    return t & 0xf;
}


// TODO - make sure that b is 0 when calling - for TXT really
INT getByte(Any *c, WORD *w, WORD *b)
{
    Type t = gtType1((*c)->types);

    if ( t == Type_Bin_Start)
    {
        if (*b != 0)
        {
            printf("ERROR - getByte called without setting b to 0\n");
            exit(0);
        }
        *c = (*c)->p1;
        *w = (WORD)(*c)->p1;
        *b = 7;
        char ch = (*w) & 0x7f;
        *w >>= 7;
        return ch;
    }
    else if (t == Type_Bin || t == Type_Bin_End)
    {
        if (((*b) + 7) > BITS)
        {
            if (t == Type_Bin_End) return 0;
            int bitsFromCurrent = BITS - (*b);
            int extra = 7 - bitsFromCurrent;
            int flag = 0x7f >> (7 - extra);
            *b = extra;
            *c = (*c)->p2;
            char ch = *w;
            *w = (WORD)(*c)->p1;
            ch |= (((*w) & flag) << bitsFromCurrent);
            *w >>= extra;
            return ch;
        }

        char ch = (*w) & 0x7f;
        (*w) >>= 7;
        (*b) += 7;
        return ch;
    }
    else if (t == Type_Txt)
    {
        if ((WORD)(*c)->p1 != *w) // TODO - this is quite not right - they could accidentally be same
        {
            *w = (WORD)(*c)->p1;
            if (*b != 0)
            {
                printf("ERROR - getByte called without setting b to 0\n");
                exit(0);
            }
        }


        char ch = ((*w) >> *b) & 0x7f;
        (*b) += 7;
        return ch;
    }
    else
    {
        printf("I dont know what this is\n");
        while(1);
    }

    return 0;
}

INT printStr(Any s)
{
    WORD w = 0;
    WORD b = 0;
    INT ch;
    while (ch = getByte(&s, &w, &b)) {printf("%c", ch);}
    return 0;
}

INT print(Any x)
{

    while (x != Nil)
    {
        Type t1 = gtType1(x->types);
        Type t2 = gtType2(x->types);

        if (t1 == Type_Sym)
        {
           print((x->p1));
        }
        else if (t1 == Type_Bin_Start || t1 == Type_Txt)
        {
            printStr(x);
            printf(" ");
        }
        else if (t1 == Type_Num)
        {
            printf("%x ", x->p1);
            return 0;
        }

        if (t2 == Type_Sym)
        {
            x = (x->p2);
        }
    }

    return 0;
}

int main()
{
    INT t;

    for (int i = 0; i < MEMS; i+=3)
    {
        printf("%llx %llx %llx\n", Mem[i], Mem[i+1], Mem[i+2]);
    }

    for (int i = 0; i < MEMS; i+=3)
    {
        Any x = (Any)&Mem[i];
        t = gtType1(x->types);
        if ( t != Type_Bin_Start && t != Type_Txt ) continue;
        print(x);
        printf("\n");
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


Any doBye()
{
    return 0;
}
