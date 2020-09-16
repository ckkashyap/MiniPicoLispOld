#include "worddefinition.h"

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
        case Type_Bin:
            printf("Type_Bin\n");
            break;
        case Type_Txt:
            printf("Type_Txt\n");
            break;
        default:
            printf("UNKNOWN\n");
    }
}

INT print(Any c)
{
    INT count = 0;
    Any v = c->p2; 

    Type t = gtType(c->types, 0);

    if (t == Type_Bin)
    {
        count++;
        c=c->p1;
        while(c)
        {
            printf("==>%p %p\n", c->p1, c->p2);
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

    return 0;
}
