#ifndef __CELL_H__
#define __CELL_H__

#include "worddefinition.h"

typedef struct Cell
{
    struct Cell *CAR;
    struct Cell *CDR;
    WORD types;
} Cell, *Any;

#endif
