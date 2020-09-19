#include "worddefinition.h"

INT BITS = (INT)sizeof(char*) * 8;

#include "mem.h"

Type gtType(WORD w, int i)
{
    char *p = (char*)&w;
    return p[i];
}

void printType(Type t)
{
    switch(t)
    {
        case Type_Undefined:
            printf("Type_Undefined\n");
            break;
        case Type_Num:
            printf("Type_Num\n");
            break;
        case Type_Sym:
            printf("Type_Sym\n");
            break;
        case Type_Pair:
            printf("Type_Pair\n");
            break;
        case Type_Bin_Start:
            printf("Type_Bin_Start\n");
            break;
        case Type_Bin_End:
            printf("Type_Bin_End\n");
            break;
        case Type_Txt:
            printf("Type_Txt\n");
            break;
        default:
            printf("UNKNOWN\n");
    }
}

// TODO - make sure that b is 0 when calling - for TXT really
INT getByte(Any *c, WORD *w, WORD *b)
{
    Type t = gtType((*c)->types, 0);

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

    if (t == Type_Bin_Start)
    {
        count++;
        c=c->p1;
        while(c)
        {
            printf("==>%p %p\n", c->p1, c->p2);
            char ch = ((WORD)c->p1&0x7f);
            printf("->%c\n", ch);
            c = c->p2;
            printf("c is now %p\n", c);
            count++;
        }
    }
    else if (t == Type_Txt)
    {
        while(1);
    }

    return count;
}

int main()
{



    Any x;
    INT ch;
    WORD w;
    WORD b=0;


    x = (Any)&Mem[0];
    w=b=0;
    while(ch=getByte(&x,  &w, &b))printf("%c", ch);printf("\n");

    x = (Any)&Mem[3];
    w=b=0;
    while(ch=getByte(&x,  &w, &b))printf("%c", ch);printf("\n");

    x = (Any)&Mem[12];
    w=b=0;
    while(ch=getByte(&x,  &w, &b))printf("%c", ch);printf("\n");

    x = (Any)&Mem[21];
    w=b=0;
    while(ch=getByte(&x,  &w, &b))printf("%c", ch);printf("\n");

    return 0;
}
