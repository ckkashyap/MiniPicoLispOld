#include "worddefinition.h"

INT BITS = (INT)sizeof(char*) * 8;

#include "sym.h"
#include "mem.h"

Type gtType(WORD w, int i)
{
    char *p = (char*)&w;
    int t = p[1];
    if (i == 0)
    {
        return t & 0xf;
    }
    else
    {
        return t >> 4;
    }
}


// TODO - make sure that b is 0 when calling - for TXT really
INT getByte(Any *c, WORD *w, WORD *b)
{
    Type t = gtType((*c)->types, 1);

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

INT print(Any c)
{
    INT count = 0;
    Any v = c->p2; 

    Type t = gtType(c->types, 0);

    return count;
}

int main()
{
    INT t;
    Any x = &Mem[3];
    WORD w = 0;
    WORD b = 0;
    INT ch;


    for (int i = 0; i < MEMS; i+=3)
    {
        x = &Mem[i];
        t = gtType(x->types, 1);
        if ( t != Type_Bin_Start && t != Type_Txt ) continue;
        w = b = 0;
        while (ch = getByte(&x, &w, &b)) {printf("%c", ch);}
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
