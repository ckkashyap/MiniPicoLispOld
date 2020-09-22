#ifndef __HEAP_H__
#define __HEAP_H__

#include  "cell.h"

#ifndef CELLS
#define CELLS (1024*1024/sizeof(Cell))
#endif

#define Free(p) ((p)->CAR=Avail, Avail=(p))

typedef struct Heap {
    Cell cells[CELLS];
    struct Heap *next;
} Heap;

#endif
