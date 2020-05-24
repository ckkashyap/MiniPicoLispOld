#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>

#include "pico.h"

typedef void (*ACTION)(void);
typedef int (*RENDERER_TYPE)(int, int, ACTION);
typedef void (*DRAWLINE_TYPE)(int, int, int, int);
typedef void (*COLOR_TYPE)(int, int, int);
typedef void (*CLEAR_TYPE)(void);
typedef void (*SWAP_TYPE)(void);

RENDERER_TYPE RENDERER;
DRAWLINE_TYPE DRAWLINE;
CLEAR_TYPE CLEAR;
SWAP_TYPE SWAP;
COLOR_TYPE COLOR;

any CB;

HANDLE DL_LOADER_HANDLE = NULL;

int getIntParam(any x)
{
    if (!isNil(x) && isNum(x))
    {
        return unBox(x);
    }
    if (!isNil(x) && isSym(x))
    {
        return unBox(val(x));
    }
    printf("CHECK OUT - ");
    print(x);
    printf("\n");
    return 100;
}

any doSDLClear(any x)
{
    CLEAR();
}

any doSDLSwap(any x)
{
    SWAP();
}

any doSDLLine(any x)
{
    int x1 = getIntParam(car(cdr(x)));
    int y1 = getIntParam(car(cdr(cdr(x))));
    int x2 = getIntParam(car(cdr(cdr(cdr(x)))));
    int y2 = getIntParam(car(cdr(cdr(cdr(cdr(x))))));
    DRAWLINE(x1, y1, x2, y2);
    return x;
}

any doSDLColor(any x)
{
    int r = getIntParam(car(cdr(x)));
    int g = getIntParam(car(cdr(cdr(x))));
    int b = getIntParam(car(cdr(cdr(cdr(x)))));
    COLOR(r, g, b);
    return x;
}


void perform()
{
    EVAL(CB);
}

any doSDL(any x)
{
    any p1 = car(cdr(x));
    any p2 = car(cdr(cdr(x)));

    int width = getIntParam(p1);
    int height = getIntParam(p2);

    DL_LOADER_HANDLE = LoadLibrary("glue.dll");

    if (DL_LOADER_HANDLE == NULL)
    {
        printf("Failed to load the DLL\n");
        return x;
    }

    RENDERER = (RENDERER_TYPE)GetProcAddress(DL_LOADER_HANDLE, "SDL_GLUE_Init");
    DRAWLINE = (DRAWLINE_TYPE)GetProcAddress(DL_LOADER_HANDLE, "SDL_GLUE_DrawLine");
    CLEAR = (CLEAR_TYPE)GetProcAddress(DL_LOADER_HANDLE, "SDL_GLUE_Clear");
    SWAP =  (SWAP_TYPE)GetProcAddress(DL_LOADER_HANDLE, "SDL_GLUE_Swap");
    COLOR =  (COLOR_TYPE)GetProcAddress(DL_LOADER_HANDLE, "SDL_GLUE_Color");


    if (RENDERER == NULL)
    {
        printf("BAD renderer\n");
        return x;
    }

    any p3 = cdr(cdr(cdr(x)));
    CB = p3;

    RENDERER(width, height, perform);

    FreeLibrary(DL_LOADER_HANDLE);
    DL_LOADER_HANDLE = NULL;

    return x;
}
