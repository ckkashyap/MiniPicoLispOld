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

INT getByte(Any *c, WORD *w, INT *b)
{
    Type t = gtType((*c)->types, 0);

    if ( t == Type_Bin_Start)
    {
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
    else
    {
        printf("I dont know what this is");
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
    }

    return count;
}

int main()
{
    for (int i = 0; i < MEMS; i+=3)
    {
        Any x = (Any)&Mem[i];
        printf("%p %llx %llx %llx\n", &Mem[i], Mem[i], Mem[i+1], Mem[i+2]);
    }
    printf("\n--\n\n");
    INT count = 0;
    for (int i = 0; i < MEMS; i+=3)
    {
        Any x = (Any)&Mem[i];
        count = print(x);
        printf("%llx %llx %llx (read %d cells)\n\n", Mem[i], Mem[i+1], Mem[i+2], count);
        i+=((count-1)*3);
    }


    INT ch;
    WORD w;
    INT b;
    Any x = (Any)&Mem[18];
    while(ch=getByte(&x,  &w, &b))
    printf("CH = %c\n", ch);

    return 0;
}
