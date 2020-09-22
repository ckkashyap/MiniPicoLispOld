#include <stdlib.h>
#include "error.h"
#include "heapmem.h"

Heap *Heaps;
Cell *Avail;

/* Allocate memory */
void *alloc(void *p, size_t siz) {
    if (!(p = realloc(p,siz)))
        giveup("No memory");
    return p;
}

void heapAlloc(void) {
   Heap *h;
   Cell *p;

   h = (Heap*)((long)alloc(NULL, sizeof(Heap) + sizeof(Cell)) + (sizeof(Cell)-1) & ~(sizeof(Cell)-1));
   h->next = Heaps,  Heaps = h;
   p = h->cells + CELLS-1;
   do
      Free(p);
   while (--p >= h->cells);
}
