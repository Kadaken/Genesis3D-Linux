/*
 * Stable C boundary for the native Linux renderer.
 *
 * The contents of this file are subject to the Genesis3D Public License
 * Version 1.01. See ../g3dlicense.txt. Contributor: Kadaken, 2026.
 */
#ifndef GENESIS3D_LINUX_RENDER_H
#define GENESIS3D_LINUX_RENDER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_LINUX_RENDER_API_VERSION 2u

typedef struct geLinuxRender_Runtime geLinuxRender_Runtime;

typedef struct geLinuxRender_Vec3 {
    double X;
    double Y;
    double Z;
} geLinuxRender_Vec3;

typedef struct geLinuxRender_Camera {
    geLinuxRender_Vec3 Position;
    double YawRadians;
    double PitchRadians;
    double MoveSpeed;
} geLinuxRender_Camera;

typedef struct geLinuxRender_Aabb {
    geLinuxRender_Vec3 Minimum;
    geLinuxRender_Vec3 Maximum;
} geLinuxRender_Aabb;

typedef struct geLinuxRender_Input {
    double MoveForward;
    double MoveRight;
    double LookDeltaX;
    double LookDeltaY;
    int Sprint;
    int Jump;
    int Fire;
    int QuitRequested;
} geLinuxRender_Input;

typedef enum geLinuxRender_HitKind {
    GE_LINUX_RENDER_HIT_NONE = 0,
    GE_LINUX_RENDER_HIT_WORLD_MODEL = 1,
    GE_LINUX_RENDER_HIT_MESH = 2,
    GE_LINUX_RENDER_HIT_ACTOR = 3
} geLinuxRender_HitKind;

typedef struct geLinuxRender_TraceResult {
    int Hit;
    geLinuxRender_HitKind Kind;
    geLinuxRender_Vec3 Impact;
    geLinuxRender_Vec3 Normal;
    double Fraction;
} geLinuxRender_TraceResult;

typedef struct geLinuxRender_Config {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char *MapPath;
    const char *ActorDirectory;
    const char *WindowTitle;
    int Width;
    int Height;
    int WindowX;
    int WindowY;
    int EnableInputCapture;
    int ShowWindow;
    int Headless;
} geLinuxRender_Config;

geLinuxRender_Runtime *geLinuxRender_Create(const geLinuxRender_Config *Config);
void geLinuxRender_Destroy(geLinuxRender_Runtime *Runtime);

int geLinuxRender_PollInput(geLinuxRender_Runtime *Runtime,
                            geLinuxRender_Input *Input);
int geLinuxRender_SetCamera(geLinuxRender_Runtime *Runtime,
                            const geLinuxRender_Camera *Camera);
int geLinuxRender_GetInitialCamera(const geLinuxRender_Runtime *Runtime,
                                   geLinuxRender_Camera *Camera);
int geLinuxRender_StepPlayer(geLinuxRender_Runtime *Runtime,
                             const geLinuxRender_Input *Input,
                             double FixedDeltaSeconds,
                             geLinuxRender_Camera *Camera);
int geLinuxRender_AdvanceSimulation(geLinuxRender_Runtime *Runtime,
                                    double FixedDeltaSeconds);
int geLinuxRender_RenderFrame(geLinuxRender_Runtime *Runtime);
int geLinuxRender_ShouldClose(const geLinuxRender_Runtime *Runtime);

int geLinuxRender_GetEntityOrigin(const geLinuxRender_Runtime *Runtime,
                                  const char *ClassName,
                                  size_t EntityIndex,
                                  geLinuxRender_Vec3 *Origin);
int geLinuxRender_GetEntityModelBounds(const geLinuxRender_Runtime *Runtime,
                                       const char *ClassName,
                                       size_t EntityIndex,
                                       geLinuxRender_Aabb *Bounds);

/* Returns API success separately from Result->Hit so a clean miss is valid. */
int geLinuxRender_TraceWorld(const geLinuxRender_Runtime *Runtime,
                             const geLinuxRender_Vec3 *Start,
                             const geLinuxRender_Vec3 *End,
                             geLinuxRender_TraceResult *Result);

const char *geLinuxRender_GetLastError(void);

#ifdef __cplusplus
}
#endif

#endif
