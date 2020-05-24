#include <stdio.h>
#include <SDL.h>

typedef void (*ACTION)(void);
SDL_Renderer* renderer = NULL;

#ifdef _MSC_VER // This identifies Microsft C compiler
#define DLL_EXPORT __declspec(dllexport) 
#else
#define DLL_EXPORT
#endif

DLL_EXPORT int SDL_GLUE_Clear()
{
    SDL_RenderClear(renderer);
    return 0;
}

DLL_EXPORT int SDL_GLUE_Swap()
{
    SDL_RenderPresent(renderer);
    return 0;
}

DLL_EXPORT int SDL_GLUE_Color(int r, int g, int b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, SDL_ALPHA_OPAQUE);
    return 0;
}

DLL_EXPORT int SDL_GLUE_DrawLine(int x1, int y1, int x2, int y2)
{
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    return 0;
}

DLL_EXPORT int SDL_GLUE_Init(int width, int height, ACTION action)
{
    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        SDL_Window* window = NULL;

        if (SDL_CreateWindowAndRenderer(width, height, 0, &window, &renderer) == 0) {
            SDL_bool done = SDL_FALSE;

            while (!done) {
                SDL_Event event;
                action();

                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        done = SDL_TRUE;
                    }
                }
            }
        }

        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = NULL;
        }
    }
    SDL_Quit();
    return 0;
}

