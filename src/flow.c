/* 29sep15abu
 * (c) Software Lab. Alexander Burger
 */

#include "pico.h"

static void redefMsg(any x, any y) {
   FILE *oSave = OutFile;

   OutFile = stderr;
   outString("# ");
   print(x);
   if (y)
      space(), print(y);
   outString(" redefined\n");
   OutFile = oSave;
}

static void redefine(any ex, any s, any x) {
   NeedSymb(ex,s);
   CheckVar(ex,s);
   if (!isNil(val(s))  &&  s != val(s)  &&  !equal(x,val(s)))
      redefMsg(s,NULL);
   val(s) = x;
}

// (quote . any) -> any
any doQuote(any x) {return cdr(x);}

// (def 'sym 'any) -> sym
// (def 'sym 'sym 'any) -> sym
any doDef(any ex) {
   any x, y;
   cell c1, c2, c3;

   x = cdr(ex),  Push(c1, EVAL(car(x)));
   NeedSymb(ex,data(c1));
   x = cdr(x),  Push(c2, EVAL(car(x)));
   if (!isCell(cdr(x))) {
      CheckVar(ex,data(c1));
      if (!isNil(y = val(data(c1)))  &&  y != data(c1)  &&  !equal(data(c2), y))
         redefMsg(data(c1),NULL);
      val(data(c1)) = data(c2);
   }
   else {
      x = cdr(x),  Push(c3, EVAL(car(x)));
      if (!isNil(y = get(data(c1), data(c2)))  &&  !equal(data(c3), y))
         redefMsg(data(c1), data(c2));
      put(data(c1), data(c2), data(c3));
   }
   return Pop(c1);
}

// (de sym . any) -> sym
any doDe(any ex) {
   redefine(ex, cadr(ex), cddr(ex));
   return cadr(ex);
}

// (meth 'obj ..) -> any
any doMeth(any ex) {
    return ex;
}

// (bye 'num|NIL)
any doBye(any ex) {
   any x = EVAL(cadr(ex));

   bye(isNil(x)? 0 : xNum(ex,x));
}
