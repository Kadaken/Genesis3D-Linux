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

#define GE_LINUX_RENDER_API_VERSION 8u

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

typedef struct geLinuxRender_FrameStats {
    uint64_t VisibleFaces;
    uint64_t SubmodelFaces;
    uint64_t ActorPrimitives;
} geLinuxRender_FrameStats;

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

typedef void (*geLinuxRender_OverlayCallback)(void *Context,
                                              int FramebufferWidth,
                                              int FramebufferHeight);

geLinuxRender_Runtime *geLinuxRender_Create(const geLinuxRender_Config *Config);
void geLinuxRender_Destroy(geLinuxRender_Runtime *Runtime);

/* Atomically replaces world-owned resources while preserving the SDL/OpenGL
 * runtime. On failure, the previous world remains active. */
int geLinuxRender_LoadMap(geLinuxRender_Runtime *Runtime,
                          const char *MapPath);

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
int geLinuxRender_GetFrameStats(const geLinuxRender_Runtime *Runtime,
                                geLinuxRender_FrameStats *Stats);

int geLinuxRender_GetEntityOrigin(const geLinuxRender_Runtime *Runtime,
                                  const char *ClassName,
                                  size_t EntityIndex,
                                  geLinuxRender_Vec3 *Origin);
int geLinuxRender_GetEntityModelBounds(const geLinuxRender_Runtime *Runtime,
                                       const char *ClassName,
                                       size_t EntityIndex,
                                       geLinuxRender_Aabb *Bounds);
int geLinuxRender_GetEntityModelIndex(const geLinuxRender_Runtime *Runtime,
                                      const char *ClassName,
                                      size_t EntityIndex,
                                      size_t *ModelIndex);

size_t geLinuxRender_GetWorldModelCount(const geLinuxRender_Runtime *Runtime);
int geLinuxRender_FindWorldModel(const geLinuxRender_Runtime *Runtime,
                                 const char *ModelName,
                                 size_t *ModelIndex);
int geLinuxRender_GetWorldModelBounds(const geLinuxRender_Runtime *Runtime,
                                      size_t ModelIndex,
                                      geLinuxRender_Aabb *Bounds);
int geLinuxRender_SetWorldModelTransform(geLinuxRender_Runtime *Runtime,
                                         size_t ModelIndex,
                                         const geLinuxRender_Vec3 *Translation,
                                         const geLinuxRender_Vec3 *EulerRadians);
int geLinuxRender_GetWorldModelMotionExtents(
    const geLinuxRender_Runtime *Runtime,
    size_t ModelIndex,
    double *StartSeconds,
    double *EndSeconds);
int geLinuxRender_SetWorldModelMotionTime(geLinuxRender_Runtime *Runtime,
                                          size_t ModelIndex,
                                          double TimeSeconds);
int geLinuxRender_SetWorldModelVisible(geLinuxRender_Runtime *Runtime,
                                       size_t ModelIndex,
                                       int Visible);

/* Generic entity inspection. String functions return the required byte count
 * including the terminating NUL, or zero when the item does not exist. */
size_t geLinuxRender_GetEntityClassCount(const geLinuxRender_Runtime *Runtime);
size_t geLinuxRender_GetEntityClassName(const geLinuxRender_Runtime *Runtime,
                                        size_t ClassIndex,
                                        char *Buffer,
                                        size_t BufferSize);
size_t geLinuxRender_GetEntityCount(const geLinuxRender_Runtime *Runtime,
                                    const char *ClassName);
size_t geLinuxRender_GetEntityKeyCount(const geLinuxRender_Runtime *Runtime,
                                       const char *ClassName,
                                       size_t EntityIndex);
size_t geLinuxRender_GetEntityKeyName(const geLinuxRender_Runtime *Runtime,
                                      const char *ClassName,
                                      size_t EntityIndex,
                                      size_t KeyIndex,
                                      char *Buffer,
                                      size_t BufferSize);
size_t geLinuxRender_GetEntityValue(const geLinuxRender_Runtime *Runtime,
                                    const char *ClassName,
                                    size_t EntityIndex,
                                    const char *Key,
                                    char *Buffer,
                                    size_t BufferSize);

/* Returns API success separately from Result->Hit so a clean miss is valid. */
int geLinuxRender_TraceWorld(const geLinuxRender_Runtime *Runtime,
                             const geLinuxRender_Vec3 *Start,
                             const geLinuxRender_Vec3 *End,
                             geLinuxRender_TraceResult *Result);
int geLinuxRender_SweepWorld(const geLinuxRender_Runtime *Runtime,
                             const geLinuxRender_Aabb *LocalBounds,
                             const geLinuxRender_Vec3 *Start,
                             const geLinuxRender_Vec3 *End,
                             geLinuxRender_TraceResult *Result);

size_t geLinuxRender_GetActorCount(const geLinuxRender_Runtime *Runtime);
int geLinuxRender_CreateActor(geLinuxRender_Runtime *Runtime,
                              const char *ActorPath,
                              const geLinuxRender_Vec3 *Position,
                              double YawRadians,
                              size_t *ActorIndex);
int geLinuxRender_RemoveActor(geLinuxRender_Runtime *Runtime,
                              size_t ActorIndex);
int geLinuxRender_ClearActors(geLinuxRender_Runtime *Runtime);
int geLinuxRender_SetActorTransform(geLinuxRender_Runtime *Runtime,
                                    size_t ActorIndex,
                                    const geLinuxRender_Vec3 *Position,
                                    double YawRadians);
int geLinuxRender_GetActorBounds(const geLinuxRender_Runtime *Runtime,
                                 size_t ActorIndex,
                                 geLinuxRender_Aabb *Bounds);
int geLinuxRender_SetActorVisible(geLinuxRender_Runtime *Runtime,
                                  size_t ActorIndex,
                                  int Visible);
size_t geLinuxRender_GetActorMotionCount(
    const geLinuxRender_Runtime *Runtime,
    size_t ActorIndex);
size_t geLinuxRender_GetActorMotionName(
    const geLinuxRender_Runtime *Runtime,
    size_t ActorIndex,
    size_t MotionIndex,
    char *Buffer,
    size_t BufferSize);
int geLinuxRender_SetActorMotion(geLinuxRender_Runtime *Runtime,
                                 size_t ActorIndex,
                                 const char *MotionName,
                                 int Restart);
int geLinuxRender_SetActorScale(geLinuxRender_Runtime *Runtime,
                                size_t ActorIndex,
                                const geLinuxRender_Vec3 *Scale);
int geLinuxRender_SetOverlayCallback(geLinuxRender_Runtime *Runtime,
                                     geLinuxRender_OverlayCallback Callback,
                                     void *Context);

const char *geLinuxRender_GetLastError(void);

#ifdef __cplusplus
}
#endif

#endif
