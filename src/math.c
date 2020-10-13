/* 25feb15abu
 * (c) Software Lab. Alexander Burger
 */

#include "pico.h"

/* Number of bytes */
int numBytes(any x) {
   int n = 4;
   word w = (word)x >> 2;

   if ((w & 0xFF000000) == 0) {
      --n;
      if ((w & 0xFF0000) == 0) {
         --n;
         if ((w & 0xFF00) == 0)
            --n;
      }
   }
   return n;
}

/* Make number from symbol */
any symToNum(any s, int scl, int sep, int ign) {
   unsigned c;
   int i;
   word w;
   bool sign, frac;
   long n;

   if (!(c = getByte1(&i, &w, &s)))
      return NULL;
   while (c <= ' ')  /* Skip white space */
      if (!(c = getByte(&i, &w, &s)))
         return NULL;
   sign = NO;
   if (c == '+'  ||  c == '-' && (sign = YES))
      if (!(c = getByte(&i, &w, &s)))
         return NULL;
   if ((c -= '0') > 9)
      return NULL;
   frac = NO;
   n = c;
   while ((c = getByte(&i, &w, &s))  &&  (!frac || scl)) {
      if ((int)c == sep) {
         if (frac)
            return NULL;
         frac = YES;
      }
      else if ((int)c != ign) {
         if ((c -= '0') > 9)
            return NULL;
         n = n * 10 + c;
         if (frac)
            --scl;
      }
   }
   if (c) {
      if ((c -= '0') > 9)
         return NULL;
      if (c >= 5)
         n += 1;
      while (c = getByte(&i, &w, &s)) {
         if ((c -= '0') > 9)
            return NULL;
      }
   }
   if (frac)
      while (--scl >= 0)
         n *= 10;
   return box(sign? -n : n);
}

/* Make symbol from number */
any numToSym(any x, int scl, int sep, int ign) {
   int i;
   word w;
   cell c1;
   long n;
   byte *p, buf[BITS/2];

   n = unBox(x);
   putByte0(&i, &w, &x);
   if (n < 0) {
      n = -n;
      putByte('-', &i, &w, &x, &c1);
   }
   for (p = buf;;) {
      *p = n % 10;
      if ((n /= 10) == 0)
         break;
      ++p;
   }
   if ((scl = p - buf - scl) < 0) {
      putByte('0', &i, &w, &x, &c1);
      putByte(sep, &i, &w, &x, &c1);
      while (scl < -1)
         putByte('0', &i, &w, &x, &c1),  ++scl;
   }
   for (;;) {
      putByte(*p + '0', &i, &w, &x, &c1);
      if (--p < buf)
         return popSym(i, w, x, &c1);
      if (scl == 0)
         putByte(sep, &i, &w, &x, &c1);
      else if (ign  &&  scl > 0  &&  scl % 3 == 0)
         putByte(ign, &i, &w, &x, &c1);
      --scl;
   }
}


// (+ 'num ..) -> num
any doAdd(any ex) {
   any x, y;
   long n;

   x = cdr(ex);
   if (isNil(y = EVAL(car(x))))
      return Nil;
   NeedNum(ex,y);
   n = unBox(y);
   while (isCell(x = cdr(x))) {
      if (isNil(y = EVAL(car(x))))
         return Nil;
      NeedNum(ex,y);
      n += unBox(y);
   }
   return box(n);
}

// (abs 'num) -> num
any doAbs(any ex) {
   any x;

   x = cdr(ex);
   if (isNil(x = EVAL(car(x))))
      return Nil;
   NeedNum(ex,x);
   return num(x)<0? box(-unBox(x)) : x;
}
