/* Neutral window-policy test; Genesis3D Public License v1.01. */
#include <SDL2/SDL.h>
#include <stdio.h>
#include "OGLDrv.h"
#include "Win32.h"
unsigned int COLOR_DEPTH, ZBUFFER_DEPTH;
void SetLastDrvError(int32 code, char *message) { (void)code; fprintf(stderr, "%s\n", message); }

int main(void)
{
    SDL_Window *window = NULL;
    DRV_DriverHook hook = {0};
    SDL_DisplayMode original, after;
    int result = 1;
    if (SDL_Init(SDL_INIT_VIDEO)) return 1;
    window = SDL_CreateWindow("Neutral driver policy", 0, 0, 320, 240,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window || SDL_GetCurrentDisplayMode(0, &original)) goto done;
    hook.hWnd = window;
    hook.Width = -1; hook.Height = 240;
    if (WindowInitialize(&hook)) goto done;
    hook.Width = 320; hook.Height = -1;
    if (WindowInitialize(&hook)) goto done;
    hook.Width = hook.Height = -1;
    if (!WindowInitialize(&hook)) goto done;
    if (WindowInitialize(&hook)) goto done; // No second active context.
    WindowCleanup();
    hook.Width = 320; hook.Height = 240; hook.Flags = 0;
    if (!WindowInitialize(&hook)) goto done;
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN_DESKTOP)
        goto done;
    if (SDL_GetCurrentDisplayMode(0, &after) || original.w != after.w || original.h != after.h)
        goto done;
    WindowCleanup();
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) goto done;
    hook.Width = original.w; hook.Height = original.h;
    hook.Flags = DRV_HOOK_EXCLUSIVE_FULLSCREEN;
    if (!WindowInitialize(&hook)) goto done;
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN)
        goto done;
    WindowCleanup();
    if (SDL_GetCurrentDisplayMode(0, &after) || original.w != after.w || original.h != after.h)
        goto done;
    result = 0;
done:
    WindowCleanup();
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    fprintf(stderr, "SDL driver policy: %s\n", result ? "FAIL" : "PASS");
    return result;
}
