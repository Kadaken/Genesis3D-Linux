/* Genesis3D Public License v1.01; see ../../../g3dlicense.txt.
 * Contributor: Kadaken, 2026. SDL host for the inherited OpenGL driver.
 * The caller creates and destroys an SDL_WINDOW_OPENGL window. This module
 * borrows that window, owns its context, and never grabs input or shows it.
 */
#include <SDL2/SDL.h>
#include <stdio.h>
#include "OGLDrv.h"
#include "Win32.h"

static SDL_Window *DriverWindow;
static SDL_GLContext DriverContext;
static geBoolean ChangedFullscreen;

geBoolean WindowInitialize(DRV_DriverHook *Hook)
{
    if (!Hook || !Hook->hWnd || DriverContext) {
        SetLastDrvError(DRV_ERROR_GENERIC, "SDL driver requires one caller-owned OpenGL window");
        return GE_FALSE;
    }
    DriverWindow = (SDL_Window *)Hook->hWnd;
    if (!(SDL_GetWindowFlags(DriverWindow) & SDL_WINDOW_OPENGL)) {
        SetLastDrvError(DRV_ERROR_GENERIC, "Create the SDL window with SDL_WINDOW_OPENGL");
        DriverWindow = NULL;
        return GE_FALSE;
    }
    if (Hook->Width != -1 && Hook->Height != -1) {
        SDL_DisplayMode requested = {0};
        requested.w = Hook->Width;
        requested.h = Hook->Height;
        if (SDL_SetWindowDisplayMode(DriverWindow, &requested) != 0 ||
            SDL_SetWindowFullscreen(DriverWindow, SDL_WINDOW_FULLSCREEN) != 0) {
            SetLastDrvError(DRV_ERROR_GENERIC, (char *)SDL_GetError());
            DriverWindow = NULL;
            return GE_FALSE;
        }
        ChangedFullscreen = GE_TRUE;
    }
    DriverContext = SDL_GL_CreateContext(DriverWindow);
    if (!DriverContext || SDL_GL_MakeCurrent(DriverWindow, DriverContext) != 0) {
        SetLastDrvError(DRV_ERROR_GENERIC, (char *)SDL_GetError());
        WindowCleanup();
        return GE_FALSE;
    }
    SDL_GL_GetDrawableSize(DriverWindow, &Hook->Width, &Hook->Height);
    COLOR_DEPTH = 24;
    ZBUFFER_DEPTH = 24;
    return GE_TRUE;
}

void WindowCleanup(void)
{
    if (DriverContext) {
        SDL_GL_DeleteContext(DriverContext);
        DriverContext = NULL;
    }
    if (DriverWindow && ChangedFullscreen)
        SDL_SetWindowFullscreen(DriverWindow, 0);
    ChangedFullscreen = GE_FALSE;
    DriverWindow = NULL;
}

void FlipGLBuffers(void)
{
    if (DriverWindow && DriverContext)
        SDL_GL_SwapWindow(DriverWindow);
}

GLint EnumNativeModes(DRV_ENUM_MODES_CB *Cb, void *Context)
{
    int display, index, count = 0;
    /* Enumeration is called before driver initialization; SDL belongs to caller. */
    /* Reserve windowed mode before the engine's fixed-size mode list fills. */
    if (!Cb(count++, "WindowMode", -1, -1, Context)) return count;
    for (display = 0; display < SDL_GetNumVideoDisplays(); ++display) {
        const int modes = SDL_GetNumDisplayModes(display);
        for (index = 0; index < modes; ++index) {
            SDL_DisplayMode mode;
            char name[80];
            if (SDL_GetDisplayMode(display, index, &mode) != 0)
                continue;
            snprintf(name, sizeof(name), "Display %d: %dx%d @ %d Hz",
                     display, mode.w, mode.h, mode.refresh_rate);
            if (!Cb(count++, name, mode.w, mode.h, Context))
                return count;
        }
    }
    return count;
}
