#ifndef __WORD_DEFINITION__
#define __WORD_DEFINITION__

#include <stdint.h>

#if INTPTR_MAX == INT32_MAX
    #define WORD_TYPE uint32_t
    #define SIGNED_WORD_TYPE int32_t
    #define WORD_FORMAT_STRING "0x%lx"
#elif INTPTR_MAX == INT64_MAX
    #define WORD_TYPE uint64_t
    #define SIGNED_WORD_TYPE int64_t
    #define WORD_FORMAT_STRING "0x%llx"
#else
    #error "Unsupported bit width"
#endif

typedef WORD_TYPE WORD;
typedef int INT;

typedef enum
{
    Type_Undefined,
    Type_Num,
    Type_Sym,
    Type_Pair,
    Type_Bin_Start,
    Type_Bin,
    Type_Bin_End,
    Type_Txt,
} Type;

typedef enum {NO,YES} BOOL;


#endif //__WORD_DEFINITION__
