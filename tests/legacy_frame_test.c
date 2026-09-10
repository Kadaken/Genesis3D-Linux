/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Asset-free legacy engine pipeline gate.
 */
#include "Genesis.h"
#include "Errorlog.h"
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neutral_bsp_fixture.h"

static const char *Stage = "SDL initialization";

static int flat_color_cases(geEngine *engine)
{
    static const unsigned char colors[][3] = {
        {255,0,0}, {0,255,0}, {0,0,255}, {128,128,128}
    };
    geRect rect = {0,319,0,239};
    geCamera *camera = NULL;
    geWorld *world = NULL;
    geBitmap *bitmap = NULL, *lock = NULL;
    gePoly *poly = NULL;
    geXForm3d transform;
    GE_LVertex vertices[4];
    unsigned char pixel[3];
    int world_added = 0, bitmap_added = 0, ok = 0;
    int color, i;
#define COLOR_CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "flat color fixture: line %d: %s\n", __LINE__, #expr); goto done; \
} } while (0)
    world = geWorld_Create(NULL);
    COLOR_CHECK(world);
    COLOR_CHECK(geEngine_AddWorld(engine, world)); world_added = 1;
    camera = geCamera_Create(2.0f, &rect);
    COLOR_CHECK(camera);
    geXForm3d_SetIdentity(&transform);
    COLOR_CHECK(geCamera_SetWorldSpaceXForm(camera, &transform));
    memset(vertices, 0, sizeof(vertices));
    for (i = 0; i < 4; ++i) {
        vertices[i].X = (i == 0 || i == 3) ? -50 : 50;
        vertices[i].Y = i < 2 ? 50 : -50;
        vertices[i].Z = -100;
        vertices[i].u = (i == 0 || i == 3) ? 0 : 1;
        vertices[i].v = i < 2 ? 0 : 1;
        vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 255;
    }
    for (color = 0; color < 4; ++color) {
        geBitmap_Info info;
        unsigned char *bits;
        bitmap = geBitmap_Create(16, 16, 1, GE_PIXELFORMAT_24BIT_RGB);
        COLOR_CHECK(bitmap && geBitmap_LockForWrite(bitmap, &lock, 0, 0));
        bits = geBitmap_GetBits(lock);
        COLOR_CHECK(bits && geBitmap_GetInfo(lock, &info, NULL));
        for (i = 0; i < info.Stride * info.Height; ++i) {
            bits[i*3] = colors[color][0];
            bits[i*3+1] = colors[color][1];
            bits[i*3+2] = colors[color][2];
        }
        COLOR_CHECK(geBitmap_UnLock(lock)); lock = NULL;
        COLOR_CHECK(geWorld_AddBitmap(world, bitmap)); bitmap_added = 1;
        poly = geWorld_AddPoly(world, vertices, 4, bitmap, GE_TEXTURED_POLY, 0, 1);
        COLOR_CHECK(poly);
        COLOR_CHECK(geEngine_BeginFrame(engine, camera, GE_TRUE));
        COLOR_CHECK(geEngine_RenderWorld(engine, world, camera, 0));
        glReadBuffer(GL_BACK);
        glReadPixels(160, 120, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
        COLOR_CHECK(geEngine_EndFrame(engine));
        COLOR_CHECK(glGetError() == GL_NO_ERROR);
        fprintf(stderr, "flat color: source=%u/%u/%u read=%u/%u/%u\n",
            colors[color][0], colors[color][1], colors[color][2],
            pixel[0], pixel[1], pixel[2]);
        COLOR_CHECK(!memcmp(pixel, colors[color], 3));
        geWorld_RemovePoly(world, poly); poly = NULL;
        COLOR_CHECK(geWorld_RemoveBitmap(world, bitmap)); bitmap_added = 0;
        geBitmap_Destroy(&bitmap);
    }
    ok = 1;
done:
    if (lock) geBitmap_UnLock(lock);
    if (poly) geWorld_RemovePoly(world, poly);
    if (bitmap_added) geWorld_RemoveBitmap(world, bitmap);
    if (bitmap) geBitmap_Destroy(&bitmap);
    if (camera) geCamera_Destroy(&camera);
    if (world_added) geEngine_RemoveWorld(engine, world);
    if (world) geWorld_Free(world);
#undef COLOR_CHECK
    return ok;
}

static int capacity_cases(geEngine *engine)
{
    const int counts[] = {63,64,75,130,123,124,75};
    geRect rect = {0,319,0,239};
    geCamera *camera = geCamera_Create(2.0f, &rect);
    geXForm3d transform;
    geWorld *world = NULL;
    geVFile *file = NULL;
    int c, frame, added = 0, ok = 0;
    unsigned char before[3], after[3];
#define CAP_CHECK(expr) do { if (!(expr)) { fprintf(stderr, "face capacity fixture: line %d: %s\n", __LINE__, #expr); goto done; } } while(0)
    CAP_CHECK(camera);
    geXForm3d_SetIdentity(&transform);
    CAP_CHECK(geCamera_SetWorldSpaceXForm(camera, &transform));
    for (c=0; c<7; ++c) {
        geVFile_MemoryContext context = {0};
        file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_MEMORY, NULL, &context, GE_VFILE_OPEN_CREATE);
        CAP_CHECK(file);
        CAP_CHECK(neutral_bsp_write_polygon(file, NULL, 0, counts[c], c == 6 ? 256 : 60,
                                            GE_PIXELFORMAT_24BIT_RGB, 0, 0));
        CAP_CHECK(geVFile_Seek(file, 0, GE_VFILE_SEEKSET));
        world = geWorld_Create(file);
        CAP_CHECK(geVFile_Close(file)); file = NULL;
        CAP_CHECK(world);
        CAP_CHECK(geEngine_AddWorld(engine, world)); added = 1;
        geErrorLog_Clear();
        for (frame=0; frame<2; ++frame) {
            CAP_CHECK(geEngine_BeginFrame(engine, camera, GE_TRUE));
            glReadBuffer(GL_BACK);
            glReadPixels(160,120,1,1,GL_RGB,GL_UNSIGNED_BYTE,before);
            CAP_CHECK(geEngine_RenderWorld(engine, world, camera, 0));
            glReadPixels(160,120,1,1,GL_RGB,GL_UNSIGNED_BYTE,after);
            CAP_CHECK(geEngine_EndFrame(engine));
            CAP_CHECK(glGetError() == GL_NO_ERROR);
            fprintf(stderr,"face capacity: vertices=%d clipped=%d frame=%d rgb=%u/%u/%u diagnostics=%d\n",
                counts[c],c==6,frame,after[0],after[1],after[2],geErrorLog_Count());
            if (counts[c] == 130 || counts[c] == 124) {
                CAP_CHECK(!memcmp(before,after,3));
                CAP_CHECK(geErrorLog_Count() == 1);
            } else {
                CAP_CHECK(abs(after[0]-12)<=2 && abs(after[1]-110)<=2 && abs(after[2]-24)<=2);
                CAP_CHECK(geErrorLog_Count() == 0);
            }
        }
        CAP_CHECK(geEngine_RemoveWorld(engine,world)); added = 0;
        geWorld_Free(world); world = NULL;
    }
    ok = 1;
done:
    if (file) geVFile_Close(file);
    if (added) geEngine_RemoveWorld(engine,world);
    if (world) geWorld_Free(world);
    if (camera) geCamera_Destroy(&camera);
#undef CAP_CHECK
    return ok;
}

static int palettized_bsp_cases(geEngine *engine)
{
    static const unsigned char expected[][3] = {
        {128,0,0}, {0,0,128}, {90,45,12}, {64,64,64}, {0,0,0}, {0,0,0}
    };
    geRect rect = {0,319,0,239};
    geCamera *camera = geCamera_Create(2.0f, &rect);
    geXForm3d transform;
    geWorld *world = NULL;
    geVFile *file = NULL;
    int c, added = 0, ok = 0;
    unsigned char pixel[3];
#define PAL_CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "palettized fixture: line %d: %s\n", __LINE__, #expr); goto done; \
} } while(0)
    PAL_CHECK(camera);
    geXForm3d_SetIdentity(&transform);
    PAL_CHECK(geCamera_SetWorldSpaceXForm(camera, &transform));
    for (c=0; c<6; ++c) {
        geVFile_MemoryContext context = {0};
        const int palette_index = c < 5 ? c + 1 : 0;
        const int transparent = palette_index == 5;
        file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_MEMORY, NULL, &context,
                                     GE_VFILE_OPEN_CREATE);
        PAL_CHECK(file);
        PAL_CHECK(c < 5
            ? neutral_bsp_write_palettized(file, palette_index, transparent)
            : neutral_bsp_write_rgb_keyed(file));
        PAL_CHECK(geVFile_Seek(file, 0, GE_VFILE_SEEKSET));
        world = geWorld_Create(file);
        PAL_CHECK(geVFile_Close(file)); file = NULL;
        PAL_CHECK(world);
        PAL_CHECK(geEngine_AddWorld(engine, world)); added = 1;
        PAL_CHECK(geEngine_BeginFrame(engine, camera, GE_TRUE));
        PAL_CHECK(geEngine_RenderWorld(engine, world, camera, 0));
        glReadBuffer(GL_BACK);
        glReadPixels(160,120,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);
        PAL_CHECK(geEngine_EndFrame(engine));
        PAL_CHECK(glGetError() == GL_NO_ERROR);
        fprintf(stderr,
            "legacy BSP texture: format=%s index=%d keyed=%d expected=%u/%u/%u read=%u/%u/%u\n",
            c < 5 ? "indexed" : "raw-rgb", palette_index, c == 5 || transparent,
            expected[c][0], expected[c][1], expected[c][2],
            pixel[0], pixel[1], pixel[2]);
        PAL_CHECK(abs((int)pixel[0] - expected[c][0]) <= 2);
        PAL_CHECK(abs((int)pixel[1] - expected[c][1]) <= 2);
        PAL_CHECK(abs((int)pixel[2] - expected[c][2]) <= 2);
        PAL_CHECK(geEngine_RemoveWorld(engine,world)); added = 0;
        geWorld_Free(world); world = NULL;
    }
    ok = 1;
done:
    if (file) geVFile_Close(file);
    if (added) geEngine_RemoveWorld(engine,world);
    if (world) geWorld_Free(world);
    if (camera) geCamera_Destroy(&camera);
#undef PAL_CHECK
    return ok;
}

static void errors(void)
{
    int i;
    geErrorLog_ErrorClassType code;
    const char *message;
    fprintf(stderr, "legacy fixture failed at %s: %s\n", Stage, SDL_GetError());
    for (i = 0; geErrorLog_Report(i, &code, &message); ++i)
        fprintf(stderr, "engine error %d: %s\n", (int)code, message ? message : "");
}

int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    geEngine *engine = NULL;
    geWorld *world = NULL;
    geCamera *camera = NULL;
    geBitmap *bitmap = NULL, *lock = NULL;
    gePoly *poly = NULL;
    geDriver *driver;
    geDriver_Mode *mode;
    geRect rect = {0, 319, 0, 239};
    geXForm3d transform;
    GE_LVertex vertices[4];
    int i, width, height, world_added = 0, bitmap_added = 0, result = 1;
    unsigned char pixel[3];

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: legacy_frame_test <driver-directory> [--bsp|--face-capacity|--flat-colors|--palette-colors]\n");
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) goto cleanup;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    Stage = "caller window creation";
    window = SDL_CreateWindow("Neutral legacy pipeline fixture", 0, 0, 320, 240,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) goto cleanup;

    Stage = "geEngine_Create";
    puts(Stage);
    engine = geEngine_Create(window, "Neutral fixture", argv[1]);
    if (!engine) goto cleanup;
    geEngine_EnableFrameRateCounter(engine, GE_FALSE);
    Stage = "driver enumeration";
    driver = geDriver_SystemGetNextDriver(geEngine_GetDriverSystem(engine), NULL);
    if (!driver) goto cleanup;
    for (mode = geDriver_GetNextMode(driver, NULL); mode;
         mode = geDriver_GetNextMode(driver, mode)) {
        if (geDriver_ModeGetWidthHeight(mode, &width, &height) &&
            width == -1 && height == -1) break;
    }
    if (!mode) goto cleanup;
    Stage = "geEngine_SetDriverAndModeNoSplash";
    puts(Stage);
    if (!geEngine_SetDriverAndModeNoSplash(engine, driver, mode)) goto cleanup;
    printf("GL renderer: %s\n", glGetString(GL_RENDERER));

    if (argc == 3 && !strcmp(argv[2], "--face-capacity")) {
        Stage = "face capacity cases";
        result = capacity_cases(engine) ? 0 : 1;
        goto cleanup;
    }
    if (argc == 3 && !strcmp(argv[2], "--flat-colors")) {
        Stage = "flat color cases";
        result = flat_color_cases(engine) ? 0 : 1;
        goto cleanup;
    }
    if (argc == 3 && !strcmp(argv[2], "--palette-colors")) {
        Stage = "palettized BSP cases";
        result = palettized_bsp_cases(engine) ? 0 : 1;
        goto cleanup;
    }

    Stage = argc == 3 ? "geWorld_Create(synthetic BSP)" : "geWorld_Create(NULL)";
    puts(Stage);
    world = argc == 3 ? neutral_bsp_world() : geWorld_Create(NULL);
    if (!world) goto cleanup;
    Stage = "geEngine_AddWorld";
    if (!geEngine_AddWorld(engine, world)) goto cleanup;
    world_added = 1;
    /* The neutral world uses the engine's public empty-world constructor and
     * a textured world polygon, ensuring a frame cannot pass by just clearing. */
    if (argc == 2) {
    bitmap = geBitmap_Create(16, 16, 1, GE_PIXELFORMAT_24BIT_RGB);
    Stage = "neutral texture creation";
    if (!bitmap || !geBitmap_LockForWrite(bitmap, &lock, 0, 0)) goto cleanup;
    {
        geBitmap_Info info;
        unsigned char *bits = geBitmap_GetBits(lock);
        if (!bits || !geBitmap_GetInfo(lock, &info, NULL)) goto cleanup;
        for (i = 0; i < info.Stride * info.Height; ++i) {
            bits[i * 3] = 24; bits[i * 3 + 1] = 220; bits[i * 3 + 2] = 48;
        }
    }
    if (!geBitmap_UnLock(lock)) goto cleanup;
    lock = NULL;
    if (!geWorld_AddBitmap(world, bitmap)) goto cleanup;
    bitmap_added = 1;
    memset(vertices, 0, sizeof(vertices));
    for (i = 0; i < 4; ++i) {
        vertices[i].X = (i == 0 || i == 3) ? -50 : 50;
        vertices[i].Y = i < 2 ? 50 : -50;
        vertices[i].Z = -100;
        vertices[i].u = (i == 0 || i == 3) ? 0 : 1;
        vertices[i].v = i < 2 ? 0 : 1;
        vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 255;
    }
    poly = geWorld_AddPoly(world, vertices, 4, bitmap, GE_TEXTURED_POLY, 0, 1);
    if (!poly) goto cleanup;
    }
    camera = geCamera_Create(2.0f, &rect);
    if (!camera) goto cleanup;
    geXForm3d_SetIdentity(&transform);
    if (!geCamera_SetWorldSpaceXForm(camera, &transform)) goto cleanup;
    Stage = "geEngine_BeginFrame";
    puts(Stage);
    if (!geEngine_BeginFrame(engine, camera, GE_TRUE)) goto cleanup;
    Stage = "geEngine_RenderWorld";
    puts(Stage);
    if (!geEngine_RenderWorld(engine, world, camera, 0)) goto cleanup;
    glReadBuffer(GL_BACK);
    glReadPixels(160, 120, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    Stage = "geEngine_EndFrame";
    puts(Stage);
    if (!geEngine_EndFrame(engine)) goto cleanup;
    printf("neutral center pixel: %u %u %u\n", pixel[0], pixel[1], pixel[2]);
    Stage = "pixel and GL error check";
    /* Half-intensity BSP lightmap must attenuate the same green texture;
     * the empty-world polygon exercises the unlightmapped texture path. */
    if (glGetError() != GL_NO_ERROR ||
        abs((int)pixel[0] - (argc == 3 ? 12 : 24)) > 2 ||
        abs((int)pixel[1] - (argc == 3 ? 110 : 220)) > 2 ||
        abs((int)pixel[2] - (argc == 3 ? 24 : 48)) > 2) goto cleanup;
    result = 0;

cleanup:
    if (result) errors();
    if (lock) geBitmap_UnLock(lock);
    if (poly) geWorld_RemovePoly(world, poly);
    if (bitmap_added) geWorld_RemoveBitmap(world, bitmap);
    if (bitmap) geBitmap_Destroy(&bitmap);
    if (camera) geCamera_Destroy(&camera);
    if (world_added) geEngine_RemoveWorld(engine, world);
    if (world) geWorld_Free(world);
    if (engine) {
        puts("geEngine_Free");
        geEngine_Free(engine);
        if (SDL_GL_GetCurrentContext() != NULL || SDL_GetWindowID(window) == 0) {
            fprintf(stderr, "legacy fixture: context/window ownership teardown failed\n");
            result = 1;
        }
    }
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    if (!result) puts("legacy frame fixture: PASS");
    return result;
}
