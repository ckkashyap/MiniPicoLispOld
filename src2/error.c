#include <stdio.h>
#include <stdlib.h>

void giveup(char *msg)
{
   fprintf(stderr, "gen: %s\n", msg);
   exit(1);
}

void noReadMacros(void)
{
   giveup("Can't support read-macros");
}

void eofErr(void)
{
   giveup("EOF Overrun");
}
