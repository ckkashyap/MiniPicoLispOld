#ifndef __WORD_DEFINITION__
#define __WORD_DEFINITION__

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

typedef UNSIGNED_WORD_TYPE WORD;
typedef int INT;

INT CELL_SIZE_BYTES = 3 * sizeof(WORD_TYPE);

typedef enum
{
    Type_Undefined,
    Type_Num,
    Type_Sym,
    Type_Pair,
    Type_Bin,
    Type_Txt,
} Type;

typedef enum {NO,YES} BOOL;

typedef struct Cell
{
    struct Cell *p1;
    struct Cell *p2;
    WORD types;
} Cell, *Any;

#endif //__WORD_DEFINITION__
