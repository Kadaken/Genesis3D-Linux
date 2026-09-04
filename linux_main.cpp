#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Genesis.h"
#include "Actor/bodyinst.h"
#include "Entities/ENTITIES.H"
#include "World/PLANE.H"
#include "World/World.h"

namespace {
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr double kPi = 3.14159265358979323846;
volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) {
    stop_requested = 1;
}

struct Vec3 {
    double x;
    double y;
    double z;
};

struct PlayerCamera {
    Vec3 position;
    double yaw;
    double pitch;
    double move_speed;
};

struct CameraBasis {
    Vec3 forward;
    Vec3 planar_forward;
    Vec3 right;
};

struct VFileRegistryGuard {
    ~VFileRegistryGuard() {
        geVFile_CloseAPI();
    }
};

struct NativeTextureSet {
    std::vector<GLuint> materials;
    std::vector<GLuint> lightmaps;
    GLuint white_lightmap = 0;
    int uploaded_materials = 0;
    int uploaded_lightmaps = 0;
    uint64_t material_rgb_sum = 0;
    uint64_t material_pixel_count = 0;
    uint64_t lightmap_rgb_sum = 0;
    uint64_t lightmap_pixel_count = 0;
    std::vector<std::vector<uint8_t>> lightmap_pixels;
    std::vector<uint8_t> lightmap_dirty;
};

struct RenderDiagnostics {
    uint64_t visible_faces = 0;
    uint64_t culled_faces = 0;
    uint64_t actor_submissions = 0;
    uint64_t translucent_submissions = 0;
    uint64_t animated_surfaces = 0;
    uint64_t dynamic_lights = 0;
    uint64_t active_light_styles = 0;
    uint64_t regenerated_lightmaps = 0;
    uint64_t lightmap_upload_bytes = 0;
    double lightmap_update_ms = 0.0;
    uint64_t animated_actor_steps = 0;
};

struct FaceCommand {
    int32 index;
    double distance_squared;
};

struct NativeFaceCache {
    struct Range { GLint first = 0; GLsizei count = 0; };
    GLuint vertex_buffer = 0;
    std::vector<Range> ranges;
};

struct CachedFaceVertex {
    GLfloat position[3];
    GLfloat material_uv[2];
    GLfloat lightmap_uv[2];
};

struct NativeActor {
    geActor_Def *definition = nullptr;
    geActor *actor = nullptr;
    geBodyInst *body_instance = nullptr;
    std::vector<GLuint> materials;
    geVec3d scale {1.0f, 1.0f, 1.0f};
    geMotion *motion = nullptr;
    geFloat motion_start = 0.0f;
    geFloat motion_end = 0.0f;
    double motion_time = 0.0;
    int motion_index = -1;
    int motion_count = 0;
    geXForm3d root_transform{};
};

struct NativeLightingState {
    static constexpr double kStyleStep = 0.1;
    double style_accumulator = 0.0;
    uint64_t style_ticks = 0;
    int32_t previous_intensity[MAX_LTYPES]{};
    bool initialized = false;
    std::vector<uint64_t> face_dynamic_mask;
};

bool upload_bitmap_texture(geBitmap *bitmap, GLuint *texture,
                           uint64_t *rgb_sum, uint64_t *pixel_count) {
    if (!bitmap || !texture)
        return false;

    geBitmap *lock = nullptr;
    if (!geBitmap_LockForReadNative(bitmap, &lock, 0, 0))
        return false;

    geBitmap_Info info {};
    uint8 *bits = static_cast<uint8 *>(geBitmap_GetBits(lock));
    if (!bits || !geBitmap_GetInfo(lock, &info, nullptr) || info.Width <= 0 ||
        info.Height <= 0 || info.Stride < info.Width) {
        geBitmap_UnLock(lock);
        return false;
    }

    const int bytes_per_pixel = gePixelFormat_BytesPerPel(info.Format);
    if (bytes_per_pixel <= 0) {
        geBitmap_UnLock(lock);
        return false;
    }

    std::vector<uint8> rgba(static_cast<size_t>(info.Width) *
                            static_cast<size_t>(info.Height) * 4U);
    for (int y = 0; y < info.Height; ++y) {
        uint8 *source = bits + static_cast<size_t>(y) * info.Stride *
                                  bytes_per_pixel;
        for (int x = 0; x < info.Width; ++x) {
            const uint32 pixel = gePixelFormat_GetPixel(info.Format, &source);
            int red, green, blue, alpha;
            if (gePixelFormat_HasPalette(info.Format)) {
                geBitmap_Palette *palette = info.Palette
                    ? info.Palette : geBitmap_GetPalette(lock);
                if (!palette || !geBitmap_Palette_GetEntryColor(
                        palette, static_cast<int>(pixel), &red, &green,
                        &blue, &alpha)) {
                    geBitmap_UnLock(lock);
                    return false;
                }
            } else {
                gePixelFormat_DecomposePixel(info.Format, pixel, &red, &green,
                                             &blue, &alpha);
            }
            const size_t destination =
                (static_cast<size_t>(y) * info.Width + x) * 4U;
            rgba[destination + 0] = static_cast<uint8>(red);
            rgba[destination + 1] = static_cast<uint8>(green);
            rgba[destination + 2] = static_cast<uint8>(blue);
            rgba[destination + 3] = static_cast<uint8>(
                info.HasColorKey && pixel == info.ColorKey ? 0 : alpha);
            *rgb_sum += static_cast<uint64_t>(red + green + blue);
            ++*pixel_count;
        }
    }

    while (glGetError() != GL_NO_ERROR) {}
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.Width, info.Height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    geBitmap_UnLock(lock);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, texture);
        *texture = 0;
        return false;
    }
    return true;
}

bool upload_world_textures(const geWorld *world, NativeTextureSet *textures) {
    if (!world || !world->CurrentBSP || !textures)
        return false;

    World_BSP *world_bsp = world->CurrentBSP;
    const GBSP_BSPData &bsp = world_bsp->BSPData;
    textures->materials.assign(static_cast<size_t>(bsp.NumGFXTextures), 0);
    textures->lightmaps.assign(static_cast<size_t>(bsp.NumGFXFaces), 0);
    textures->lightmap_pixels.resize(static_cast<size_t>(bsp.NumGFXFaces));
    textures->lightmap_dirty.assign(static_cast<size_t>(bsp.NumGFXFaces), 0);
    // Genesis3D lightmaps are tightly packed RGB rows; widths are not
    // guaranteed to satisfy OpenGL's default four-byte unpack alignment.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const uint8 white[3] = {255, 255, 255};
    glGenTextures(1, &textures->white_lightmap);
    glBindTexture(GL_TEXTURE_2D, textures->white_lightmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, white);

    for (int32 index = 0; index < bsp.NumGFXTextures; ++index) {
        geBitmap *bitmap = geWBitmap_Pool_GetBitmapByIndex(
            world_bsp->WBitmapPool, index);
        if (upload_bitmap_texture(bitmap, &textures->materials[index],
                                  &textures->material_rgb_sum,
                                  &textures->material_pixel_count))
            ++textures->uploaded_materials;
    }

    for (int32 index = 0; index < bsp.NumGFXFaces; ++index) {
        const GFX_Face &face = bsp.GFXFaces[index];
        if (face.LightOfs < 0 || face.LWidth <= 0 || face.LHeight <= 0)
            continue;

        const size_t pixel_count = static_cast<size_t>(face.LWidth) *
                                   static_cast<size_t>(face.LHeight);
        const size_t offset = static_cast<size_t>(face.LightOfs) + 1U;
        const size_t byte_count = pixel_count * 3U;
        if (!bsp.GFXLightData || offset > static_cast<size_t>(bsp.NumGFXLightData) ||
            byte_count > static_cast<size_t>(bsp.NumGFXLightData) - offset)
            continue;

        GLuint &texture = textures->lightmaps[index];
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, face.LWidth, face.LHeight, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, bsp.GFXLightData + offset);
        if (glGetError() == GL_NO_ERROR) {
            ++textures->uploaded_lightmaps;
            const uint8 *light_pixels = bsp.GFXLightData + offset;
            for (size_t byte = 0; byte < byte_count; ++byte)
                textures->lightmap_rgb_sum += light_pixels[byte];
            textures->lightmap_pixel_count += pixel_count;
            textures->lightmap_pixels[index].assign(
                bsp.GFXLightData + offset,
                bsp.GFXLightData + offset + byte_count);
        } else {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return textures->uploaded_materials > 0;
}

void destroy_world_textures(NativeTextureSet *textures) {
    if (!textures)
        return;
    if (!textures->materials.empty())
        glDeleteTextures(static_cast<GLsizei>(textures->materials.size()),
                         textures->materials.data());
    if (!textures->lightmaps.empty())
        glDeleteTextures(static_cast<GLsizei>(textures->lightmaps.size()),
                         textures->lightmaps.data());
    if (textures->white_lightmap)
        glDeleteTextures(1, &textures->white_lightmap);
    textures->materials.clear();
    textures->lightmaps.clear();
}

bool build_face_cache(const geWorld *world, NativeFaceCache *cache) {
    if (!world || !world->CurrentBSP || !cache)
        return false;
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    cache->ranges.resize(static_cast<size_t>(bsp.NumGFXFaces));
    std::vector<CachedFaceVertex> vertices;
    for (int32 face_index = 0; face_index < bsp.NumGFXFaces; ++face_index) {
        const GFX_Face &face = bsp.GFXFaces[face_index];
        if (face.NumVerts < 3 || face.TexInfo < 0 ||
            face.TexInfo >= bsp.NumGFXTexInfo)
            continue;
        const GFX_TexInfo &texture_info = bsp.GFXTexInfo[face.TexInfo];
        if (texture_info.Texture < 0 || texture_info.Texture >= bsp.NumGFXTextures)
            continue;
        const GFX_Texture &metadata = bsp.GFXTextures[texture_info.Texture];
        if (metadata.Width <= 0 || metadata.Height <= 0)
            continue;
        const Surf_SurfInfo &surface = world->CurrentBSP->SurfInfo[face_index];
        const float scale_u = std::fabs(texture_info.DrawScale[0]) > 1.0e-6f
            ? texture_info.DrawScale[0] : 1.0f;
        const float scale_v = std::fabs(texture_info.DrawScale[1]) > 1.0e-6f
            ? texture_info.DrawScale[1] : 1.0f;
        NativeFaceCache::Range &range = cache->ranges[face_index];
        range.first = static_cast<GLint>(vertices.size());
        for (int32 corner = 0; corner < face.NumVerts; ++corner) {
            const int32 list_index = face.FirstVert + corner;
            if (list_index < 0 || list_index >= bsp.NumGFXVertIndexList)
                continue;
            const int32 vertex_index = bsp.GFXVertIndexList[list_index];
            if (vertex_index < 0 || vertex_index >= bsp.NumGFXVerts)
                continue;
            const geVec3d &vertex = bsp.GFXVerts[vertex_index];
            const Surf_TexVert &texvert = world->CurrentBSP->TexVerts[list_index];
            CachedFaceVertex cached = {
                {vertex.X, vertex.Y, vertex.Z},
                {(texvert.u / scale_u + surface.ShiftU) / metadata.Width,
                 (texvert.v / scale_v + surface.ShiftV) / metadata.Height},
                {face.LWidth > 0 ? (texvert.u - surface.LInfo.MinU + 8.0f) /
                    (static_cast<float>(face.LWidth) * 16.0f) : 0.5f,
                 face.LHeight > 0 ? (texvert.v - surface.LInfo.MinV + 8.0f) /
                    (static_cast<float>(face.LHeight) * 16.0f) : 0.5f}};
            vertices.push_back(cached);
        }
        range.count = static_cast<GLsizei>(vertices.size()) - range.first;
    }
    glGenBuffers(1, &cache->vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, cache->vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return glGetError() == GL_NO_ERROR;
}

void destroy_face_cache(NativeFaceCache *cache) {
    if (cache && cache->vertex_buffer)
        glDeleteBuffers(1, &cache->vertex_buffer);
    if (cache) {
        cache->vertex_buffer = 0;
        cache->ranges.clear();
    }
}

bool load_native_actor(const std::string &path, NativeActor *native_actor) {
    if (!native_actor)
        return false;
    geVFile *file = geVFile_OpenNewSystem(nullptr, GE_VFILE_TYPE_DOS,
                                           path.c_str(), nullptr,
                                           GE_VFILE_OPEN_READONLY);
    if (!file)
        return false;
    native_actor->definition = geActor_DefCreateFromFile(file);
    geVFile_Close(file);
    if (!native_actor->definition)
        return false;
    native_actor->actor = geActor_Create(native_actor->definition);
    if (!native_actor->actor)
        return false;
    geXForm3d_SetIdentity(&native_actor->root_transform);
    geActor_ClearPose(native_actor->actor, &native_actor->root_transform);
    const int motion_count = geActor_GetMotionCount(native_actor->definition);
    native_actor->motion_count = motion_count;
    for (int motion_index = 0; motion_index < motion_count; ++motion_index) {
        geMotion *motion = geActor_GetMotionByIndex(native_actor->definition,
                                                     motion_index);
        geFloat start = 0.0f, end = 0.0f;
        if (motion && geMotion_GetTimeExtents(motion, &start, &end) &&
            end > start) {
            native_actor->motion = motion;
            native_actor->motion_start = start;
            native_actor->motion_end = end;
            native_actor->motion_time = start;
            native_actor->motion_index = motion_index;
            break;
        }
    }
    geBody *body = geActor_GetBody(native_actor->definition);
    native_actor->body_instance = geBodyInst_Create(body);
    if (!native_actor->body_instance)
        return false;

    const int material_count = geBody_GetMaterialCount(body);
    native_actor->materials.assign(static_cast<size_t>(material_count), 0);
    uint64_t ignored_sum = 0;
    uint64_t ignored_pixels = 0;
    for (int material = 0; material < material_count; ++material) {
        const char *name = nullptr;
        geBitmap *bitmap = nullptr;
        geFloat red = 255.0f, green = 255.0f, blue = 255.0f;
        if (geBody_GetMaterial(body, material, &name, &bitmap,
                               &red, &green, &blue) && bitmap)
            upload_bitmap_texture(bitmap, &native_actor->materials[material],
                                  &ignored_sum, &ignored_pixels);
    }
    return true;
}

void set_actor_root_transform(NativeActor *actor, const geXForm3d &transform) {
    if (!actor || !actor->actor)
        return;
    actor->root_transform = transform;
    if (actor->motion)
        geActor_SetPose(actor->actor, actor->motion,
                        static_cast<geFloat>(actor->motion_time),
                        &actor->root_transform);
    else
        geActor_ClearPose(actor->actor, &actor->root_transform);
}

void advance_actor_motion(NativeActor *actor, double fixed_dt,
                          RenderDiagnostics *diagnostics) {
    if (!actor || !actor->actor || !actor->motion)
        return;
    const double duration = actor->motion_end - actor->motion_start;
    if (duration <= 0.0)
        return;
    actor->motion_time += fixed_dt;
    actor->motion_time = actor->motion_start +
        std::fmod(actor->motion_time - actor->motion_start, duration);
    geActor_SetPose(actor->actor, actor->motion,
                    static_cast<geFloat>(actor->motion_time),
                    &actor->root_transform);
    if (diagnostics)
        ++diagnostics->animated_actor_steps;
}

int32_t light_style_intensity(const Light_LightInfo &lights, int style,
                              uint64_t tick, bool *animated) {
    if (style == 0)
        return 1 << 8;
    if (style < 0 || style >= MAX_LTYPES)
        return 0;
    const char *table = lights.LTypeTable[style];
    const size_t length = strnlen(table, sizeof(lights.LTypeTable[style]));
    if (length == 0)
        return 1 << 8;
    if (animated)
        *animated = length > 1;
    const int value = (std::max)(0, (std::min)(27,
        static_cast<int>(table[tick % length]) - 96));
    return (value << 8) / 27;
}

void advance_light_styles(const geWorld *world, NativeTextureSet *textures,
                          NativeLightingState *state, double fixed_dt,
                          RenderDiagnostics *diagnostics) {
    if (!world || !world->CurrentBSP || !world->LightInfo || !textures || !state)
        return;
    state->style_accumulator += fixed_dt;
    bool ticked = false;
    while (state->style_accumulator + 1.0e-12 >= NativeLightingState::kStyleStep) {
        state->style_accumulator -= NativeLightingState::kStyleStep;
        ++state->style_ticks;
        ticked = true;
    }
    if (!ticked && state->initialized)
        return;
    bool changed[MAX_LTYPES]{};
    uint64_t active = 0;
    for (int style = 0; style < MAX_LTYPES; ++style) {
        bool animated = false;
        const int32_t intensity = light_style_intensity(
            *world->LightInfo, style, state->style_ticks, &animated);
        changed[style] = !state->initialized ||
                         intensity != state->previous_intensity[style];
        state->previous_intensity[style] = intensity;
        active += animated ? 1U : 0U;
    }
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    for (int32 face_index = 0; face_index < bsp.NumGFXFaces; ++face_index) {
        const GFX_Face &face = bsp.GFXFaces[face_index];
        for (int layer = 0; layer < 4 && face.LTypes[layer] != 255; ++layer) {
            if (face.LTypes[layer] < MAX_LTYPES && changed[face.LTypes[layer]]) {
                textures->lightmap_dirty[face_index] = 1;
                break;
            }
        }
    }
    state->initialized = true;
    if (diagnostics)
        diagnostics->active_light_styles = (std::max)(
            diagnostics->active_light_styles, active);
}

void mark_dynamic_lightmaps(const geWorld *world, NativeTextureSet *textures,
                            NativeLightingState *state) {
    if (!world || !world->CurrentBSP || !world->LightInfo || !textures || !state)
        return;
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    if (state->face_dynamic_mask.size() != static_cast<size_t>(bsp.NumGFXFaces))
        state->face_dynamic_mask.assign(static_cast<size_t>(bsp.NumGFXFaces), 0);
    if (world->LightInfo->NumDynamicLights == 0 &&
        std::none_of(state->face_dynamic_mask.begin(), state->face_dynamic_mask.end(),
                     [](uint64_t mask) { return mask != 0; }))
        return;
    for (int32 face_index = 0; face_index < bsp.NumGFXFaces; ++face_index) {
        const GFX_Face &face = bsp.GFXFaces[face_index];
        const Surf_SurfInfo &surface = world->CurrentBSP->SurfInfo[face_index];
        uint64_t mask = 0;
        if (face.LightOfs >= 0 && face.PlaneNum >= 0 &&
            face.PlaneNum < bsp.NumGFXPlanes) {
            const GFX_Plane &plane = bsp.GFXPlanes[face.PlaneNum];
            for (int light_index = 0; light_index < MAX_DYNAMIC_LIGHTS;
                 ++light_index) {
                const Light_DLight &light = world->LightInfo->DynamicLights[light_index];
                if (!light.Active || light.Radius <= 0.0f)
                    continue;
                const float plane_distance = std::fabs(
                    geVec3d_DotProduct(&light.Pos, &plane.Normal) - plane.Dist);
                if (plane_distance >= light.Radius ||
                    light.Pos.X + light.Radius < surface.VMins.X ||
                    light.Pos.X - light.Radius > surface.VMaxs.X ||
                    light.Pos.Y + light.Radius < surface.VMins.Y ||
                    light.Pos.Y - light.Radius > surface.VMaxs.Y ||
                    light.Pos.Z + light.Radius < surface.VMins.Z ||
                    light.Pos.Z - light.Radius > surface.VMaxs.Z)
                    continue;
                mask |= uint64_t{1} << light_index;
            }
        }
        const uint64_t previous = state->face_dynamic_mask[face_index];
        if (mask || previous != mask)
            textures->lightmap_dirty[face_index] = 1;
        state->face_dynamic_mask[face_index] = mask;
    }
}

void composite_dynamic_lights(const geWorld *world, int32 face_index,
                              uint64_t mask, std::vector<int32_t> *light) {
    if (!world || !world->CurrentBSP || !world->LightInfo || !light || !mask)
        return;
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    const GFX_Face &face = bsp.GFXFaces[face_index];
    const Surf_SurfInfo &surface = world->CurrentBSP->SurfInfo[face_index];
    if (face.TexInfo < 0 || face.TexInfo >= bsp.NumGFXTexInfo ||
        face.PlaneNum < 0 || face.PlaneNum >= bsp.NumGFXPlanes)
        return;
    const GFX_TexInfo &tex = bsp.GFXTexInfo[face.TexInfo];
    const GFX_Plane &plane = bsp.GFXPlanes[face.PlaneNum];
    for (int light_index = 0; light_index < MAX_DYNAMIC_LIGHTS; ++light_index) {
        if (!(mask & (uint64_t{1} << light_index)))
            continue;
        const Light_DLight &dynamic = world->LightInfo->DynamicLights[light_index];
        float radius = dynamic.Radius - std::fabs(
            geVec3d_DotProduct(&dynamic.Pos, &plane.Normal) - plane.Dist);
        if (radius <= 0.0f)
            continue;
        if (dynamic.Spot) {
            geVec3d plane_normal = plane.Normal;
            if (face.PlaneSide)
                geVec3d_Inverse(&plane_normal);
            geVec3d row = surface.TexOrg;
            for (int y_index = 0; y_index < face.LHeight; ++y_index) {
                geVec3d sample = row;
                for (int x_index = 0; x_index < face.LWidth; ++x_index) {
                    geVec3d direction;
                    geVec3d_Subtract(&dynamic.Pos, &sample, &direction);
                    const float distance = geVec3d_Normalize(&direction);
                    const float incidence = geVec3d_DotProduct(
                        &direction, &plane_normal);
                    const float cone = -geVec3d_DotProduct(
                        &direction, &dynamic.Normal);
                    if (incidence >= 0.001f && distance < dynamic.Radius &&
                        cone >= dynamic.Angle) {
                        float value = (cone - dynamic.Angle) /
                            (1.001f - dynamic.Angle);
                        if (dynamic.Style == 1)
                            value *= value;
                        else if (dynamic.Style == 2)
                            value = std::sqrt((std::max)(0.0f, value));
                        value *= dynamic.Radius - distance;
                        const size_t pixel = (static_cast<size_t>(y_index) *
                            face.LWidth + x_index) * 3U;
                        (*light)[pixel + 0] += static_cast<int>(value * dynamic.FColorR);
                        (*light)[pixel + 1] += static_cast<int>(value * dynamic.FColorG);
                        (*light)[pixel + 2] += static_cast<int>(value * dynamic.FColorB);
                    }
                    geVec3d_Add(&sample, &surface.T2WVecs[0], &sample);
                }
                geVec3d_Add(&row, &surface.T2WVecs[1], &row);
            }
            continue;
        }
        const int sx = (static_cast<int>(geVec3d_DotProduct(&dynamic.Pos,
            &tex.Vecs[0])) - surface.LInfo.MinU) * surface.XScale;
        const int sy = (static_cast<int>(geVec3d_DotProduct(&dynamic.Pos,
            &tex.Vecs[1])) - surface.LInfo.MinV) * surface.YScale;
        int fixed_y = sy;
        for (int y_index = 0; y_index < face.LHeight; ++y_index) {
            const int y = std::abs(fixed_y >> 10);
            int fixed_x = sx;
            for (int x_index = 0; x_index < face.LWidth; ++x_index) {
                const int x = std::abs(fixed_x >> 10);
                const int distance = x > y ? x + (y >> 1) : y + (x >> 1);
                if (distance < static_cast<int>(radius)) {
                    const int value = static_cast<int>(radius) - distance;
                    const size_t pixel = (static_cast<size_t>(y_index) *
                        face.LWidth + x_index) * 3U;
                    (*light)[pixel + 0] += static_cast<int>(value * dynamic.FColorR);
                    (*light)[pixel + 1] += static_cast<int>(value * dynamic.FColorG);
                    (*light)[pixel + 2] += static_cast<int>(value * dynamic.FColorB);
                }
                fixed_x -= surface.XStep;
            }
            fixed_y -= surface.YStep;
        }
    }
}

void regenerate_visible_lightmaps(const geWorld *world,
                                  NativeTextureSet *textures,
                                  const NativeLightingState &state,
                                  const std::vector<FaceCommand> &commands,
                                  RenderDiagnostics *diagnostics) {
    if (!world || !world->CurrentBSP || !world->LightInfo || !textures)
        return;
    if (std::none_of(commands.begin(), commands.end(),
        [textures](const FaceCommand &command) {
            return command.index >= 0 &&
                command.index < static_cast<int32>(textures->lightmap_dirty.size()) &&
                textures->lightmap_dirty[command.index] != 0;
        }))
        return;
    const auto started = std::chrono::steady_clock::now();
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (const FaceCommand &command : commands) {
        const int32 index = command.index;
        if (index < 0 || index >= bsp.NumGFXFaces ||
            !textures->lightmap_dirty[index] || !textures->lightmaps[index])
            continue;
        const GFX_Face &face = bsp.GFXFaces[index];
        const size_t pixels = static_cast<size_t>(face.LWidth) * face.LHeight;
        const size_t layer_bytes = pixels * 3U;
        const size_t offset = static_cast<size_t>(face.LightOfs) + 1U;
        if (face.LightOfs < 0 || offset > static_cast<size_t>(bsp.NumGFXLightData))
            continue;
        std::vector<uint8_t> &output = textures->lightmap_pixels[index];
        std::vector<int32_t> accumulated(layer_bytes, 0);
        for (int layer = 0; layer < 4 && face.LTypes[layer] != 255; ++layer) {
            const size_t source_offset = offset + static_cast<size_t>(layer) * layer_bytes;
            if (source_offset > static_cast<size_t>(bsp.NumGFXLightData) ||
                layer_bytes > static_cast<size_t>(bsp.NumGFXLightData) - source_offset)
                break;
            const int intensity = light_style_intensity(
                *world->LightInfo, face.LTypes[layer], state.style_ticks, nullptr);
            for (size_t byte = 0; byte < layer_bytes; ++byte) {
                accumulated[byte] +=
                    static_cast<int>(bsp.GFXLightData[source_offset + byte]) *
                    intensity;
            }
        }
        const uint64_t dynamic_mask = index < static_cast<int32>(state.face_dynamic_mask.size())
            ? state.face_dynamic_mask[index] : 0;
        composite_dynamic_lights(world, index, dynamic_mask, &accumulated);
        output.resize(layer_bytes);
        for (size_t byte = 0; byte < layer_bytes; ++byte)
            output[byte] = static_cast<uint8_t>((std::max)(0,
                (std::min)(255, accumulated[byte] >> 8)));
        glBindTexture(GL_TEXTURE_2D, textures->lightmaps[index]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, face.LWidth, face.LHeight,
                        GL_RGB, GL_UNSIGNED_BYTE, output.data());
        textures->lightmap_dirty[index] = 0;
        if (diagnostics) {
            ++diagnostics->regenerated_lightmaps;
            diagnostics->lightmap_upload_bytes += layer_bytes;
        }
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (diagnostics)
        diagnostics->lightmap_update_ms += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
}

bool validate_dynamic_light_cycle(geWorld *world, NativeTextureSet *textures,
                                  NativeLightingState *state) {
    if (!world || !world->CurrentBSP || !textures || !state)
        return false;
    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    int32 seed_face = -1;
    geVec3d position{};
    for (int32 face_index = 0; face_index < bsp.NumGFXFaces; ++face_index) {
        const GFX_Face &face = bsp.GFXFaces[face_index];
        if (face.LightOfs < 0 || face.NumVerts < 1 || face.FirstVert < 0 ||
            face.FirstVert >= bsp.NumGFXVertIndexList)
            continue;
        const int32 vertex = bsp.GFXVertIndexList[face.FirstVert];
        if (vertex >= 0 && vertex < bsp.NumGFXVerts) {
            seed_face = face_index;
            position = bsp.GFXVerts[vertex];
            break;
        }
    }
    if (seed_face < 0)
        return false;
    geLight *light = geWorld_AddLight(world);
    const GE_RGBA color {255.0f, 160.0f, 80.0f, 255.0f};
    if (!light || !geWorld_SetLightAttributes(world, light, &position, &color,
                                               256.0f, GE_FALSE)) {
        if (light)
            geWorld_RemoveLight(world, light);
        return false;
    }
    mark_dynamic_lightmaps(world, textures, state);
    int32 affected = -1;
    for (int32 face_index = 0; face_index < bsp.NumGFXFaces; ++face_index) {
        if (state->face_dynamic_mask[face_index] &&
            !textures->lightmap_pixels[face_index].empty()) {
            affected = face_index;
            break;
        }
    }
    if (affected < 0) {
        geWorld_RemoveLight(world, light);
        mark_dynamic_lightmaps(world, textures, state);
        return false;
    }
    const std::vector<uint8_t> baseline = textures->lightmap_pixels[affected];
    std::vector<FaceCommand> command {{affected, 0.0}};
    RenderDiagnostics ignored;
    regenerate_visible_lightmaps(world, textures, *state, command, &ignored);
    const bool brightened = textures->lightmap_pixels[affected] != baseline;
    geWorld_RemoveLight(world, light);
    mark_dynamic_lightmaps(world, textures, state);
    regenerate_visible_lightmaps(world, textures, *state, command, &ignored);
    const bool restored = textures->lightmap_pixels[affected] == baseline;
    std::fprintf(stderr,
        "Genesis3D Linux: dynamic lightmap validation: face %d, add %s, "
        "remove/restore %s\n", affected, brightened ? "PASS" : "FAIL",
        restored ? "PASS" : "FAIL");
    return brightened && restored;
}

void destroy_native_actor(NativeActor *native_actor) {
    if (!native_actor)
        return;
    if (!native_actor->materials.empty())
        glDeleteTextures(static_cast<GLsizei>(native_actor->materials.size()),
                         native_actor->materials.data());
    if (native_actor->body_instance)
        geBodyInst_Destroy(&native_actor->body_instance);
    if (native_actor->actor)
        geActor_Destroy(&native_actor->actor);
    if (native_actor->definition)
        geActor_DefDestroy(&native_actor->definition);
}

Vec3 subtract(const Vec3 &a, const Vec3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

Vec3 normalized(const Vec3 &value) {
    const double length = std::sqrt(value.x * value.x + value.y * value.y +
                                    value.z * value.z);
    if (length <= 1.0e-12)
        return {0.0, 0.0, 0.0};
    return {value.x / length, value.y / length, value.z / length};
}

CameraBasis camera_basis(const PlayerCamera &camera) {
    const double yaw_cosine = std::cos(camera.yaw);
    const double yaw_sine = std::sin(camera.yaw);
    const double pitch_cosine = std::cos(camera.pitch);
    return {{pitch_cosine * yaw_cosine, std::sin(camera.pitch),
             pitch_cosine * yaw_sine},
            {yaw_cosine, 0.0, yaw_sine},
            {-yaw_sine, 0.0, yaw_cosine}};
}

PlayerCamera initial_camera(geWorld *world) {
    geEntity_EntitySet *player_set = geWorld_GetEntitySet(world, "PlayerSetup");
    geEntity *player = player_set
        ? geEntity_EntitySetGetNextEntity(player_set, nullptr) : nullptr;
    const char *player_origin = player
        ? geEntity_GetStringForKey(player, "origin") : nullptr;
    geVec3d parsed_origin{};
    if (player_origin && std::sscanf(player_origin, "%f %f %f",
                                    &parsed_origin.X, &parsed_origin.Y,
                                    &parsed_origin.Z) == 3) {
        return {{parsed_origin.X, parsed_origin.Y, parsed_origin.Z},
                0.0, 0.0, 400.0};
    }

    const GFX_Model &root_model = world->CurrentBSP->BSPData.GFXModels[0];
    const Vec3 minimum = {root_model.Mins.X, root_model.Mins.Y,
                          root_model.Mins.Z};
    const Vec3 maximum = {root_model.Maxs.X, root_model.Maxs.Y,
                          root_model.Maxs.Z};
    const Vec3 center = {(minimum.x + maximum.x) * 0.5,
                         (minimum.y + maximum.y) * 0.5,
                         (minimum.z + maximum.z) * 0.5};
    const Vec3 extent = subtract(maximum, minimum);
    const double radius = (std::max)(1.0, 0.5 * std::sqrt(
        extent.x * extent.x + extent.y * extent.y + extent.z * extent.z));
    const Vec3 position = {center.x + radius * 1.35,
                           center.y + radius * 0.55,
                           center.z + radius * 1.35};
    const Vec3 direction = normalized(subtract(center, position));
    return {position,
            std::atan2(direction.z, direction.x),
            std::asin((std::max)(-1.0, (std::min)(1.0, direction.y))),
            (std::max)(200.0, radius * 0.35)};
}

void update_camera_movement(PlayerCamera *camera, const CameraBasis &basis,
                            const Uint8 *keys, double delta_seconds) {
    if (!camera || !keys)
        return;

    const double forward_axis =
        (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP] ? 1.0 : 0.0) -
        (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? 1.0 : 0.0);
    const double strafe_axis =
        (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] ? 1.0 : 0.0) -
        (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] ? 1.0 : 0.0);

    Vec3 movement = {
        basis.planar_forward.x * forward_axis + basis.right.x * strafe_axis,
        0.0,
        basis.planar_forward.z * forward_axis + basis.right.z * strafe_axis
    };
    movement = normalized(movement);
    const double sprint =
        keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] ? 2.0 : 1.0;
    const double distance = camera->move_speed * sprint * delta_seconds;
    camera->position.x += movement.x * distance;
    camera->position.z += movement.z * distance;
}

void look_at(const Vec3 &eye, const Vec3 &target) {
    const Vec3 forward = normalized(subtract(target, eye));
    const Vec3 side = normalized(cross(forward, {0.0, 1.0, 0.0}));
    const Vec3 up = cross(side, forward);
    const GLfloat view[16] = {
        static_cast<GLfloat>(side.x), static_cast<GLfloat>(up.x),
        static_cast<GLfloat>(-forward.x), 0.0f,
        static_cast<GLfloat>(side.y), static_cast<GLfloat>(up.y),
        static_cast<GLfloat>(-forward.y), 0.0f,
        static_cast<GLfloat>(side.z), static_cast<GLfloat>(up.z),
        static_cast<GLfloat>(-forward.z), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    glMultMatrixf(view);
    glTranslatef(static_cast<GLfloat>(-eye.x), static_cast<GLfloat>(-eye.y),
                 static_cast<GLfloat>(-eye.z));
}

bool render_native_actor(NativeActor *native_actor,
                         RenderDiagnostics *diagnostics) {
    if (!native_actor || !native_actor->actor ||
        !native_actor->body_instance)
        return false;
    const geXFArray *transforms =
        geActor_GetPoseTransforms(native_actor->actor);
    const geBodyInst_Geometry *geometry = geBodyInst_GetGeometry(
        native_actor->body_instance, &native_actor->scale, transforms,
        GE_BODY_HIGHEST_LOD, nullptr);
    if (!geometry || !geometry->FaceList || !geometry->SkinVertexArray)
        return false;

    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const geBodyInst_Index *cursor = geometry->FaceList;
    if (geometry->FaceListSize < 0 ||
        geometry->FaceListSize % static_cast<int32>(sizeof(*cursor)) != 0)
        return false;
    const geBodyInst_Index *end = cursor +
        geometry->FaceListSize / static_cast<int32>(sizeof(*cursor));
    while (cursor < end) {
        const int primitive = *cursor++;
        if (cursor >= end)
            return false;
        const int material = *cursor++;
        int vertex_count = 3;
        GLenum mode = GL_TRIANGLES;
        if (primitive == GE_BODYINST_FACE_TRISTRIP ||
            primitive == GE_BODYINST_FACE_TRIFAN) {
            if (cursor >= end)
                return false;
            vertex_count = static_cast<int>(*cursor++) + 2;
            mode = primitive == GE_BODYINST_FACE_TRISTRIP
                ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN;
        } else if (primitive != GE_BODYINST_FACE_TRIANGLE) {
            return false;
        }
        if (vertex_count < 3 || end - cursor < vertex_count * 2)
            return false;
        const GLuint texture = material >= 0 &&
            material < static_cast<int>(native_actor->materials.size())
            ? native_actor->materials[material] : 0;
        glBindTexture(GL_TEXTURE_2D, texture);
        glBegin(mode);
        for (int index = 0; index < vertex_count; ++index) {
            const int vertex_index = *cursor++;
            ++cursor; // normal index; fixed-function lighting is not enabled
            if (vertex_index < 0 || vertex_index >= geometry->SkinVertexCount)
                continue;
            const geBodyInst_SkinVertex &vertex =
                geometry->SkinVertexArray[vertex_index];
            glTexCoord2f(vertex.SVU, vertex.SVV);
            glVertex3f(vertex.SVPoint.X, vertex.SVPoint.Y, vertex.SVPoint.Z);
        }
        glEnd();
        ++diagnostics->actor_submissions;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool validate_actor_fixtures(const std::vector<std::string> &paths,
                             const geXForm3d &base_transform,
                             RenderDiagnostics *diagnostics) {
    if (paths.size() != 4)
        return false;
    for (size_t fixture = 0; fixture < paths.size(); ++fixture) {
        NativeActor actor;
        if (!load_native_actor(paths[fixture], &actor))
            return false;
        geBody *body = geActor_GetBody(actor.definition);
        int vertices = 0, faces = 0, normals = 0;
        geXForm3d transform = base_transform;
        transform.Translation.X += static_cast<geFloat>(fixture * 36.0);
        set_actor_root_transform(&actor, transform);
        const geXFArray *pose = geActor_GetPoseTransforms(actor.actor);
        const geBodyInst_Geometry *geometry = geBodyInst_GetGeometry(
            actor.body_instance, &actor.scale, pose, GE_BODY_HIGHEST_LOD, nullptr);
        const bool valid = geBody_GetGeometryStats(body, GE_BODY_HIGHEST_LOD,
                &vertices, &faces, &normals) && vertices > 0 && faces > 0 &&
            normals > 0 && geBody_GetMaterialCount(body) > 0 && geometry &&
            geometry->FaceList && geometry->SkinVertexArray &&
            render_native_actor(&actor, diagnostics);
        bool pose_changes = false;
        float maximum_vertex_delta = 0.0f;
        if (valid && actor.motion) {
            std::vector<geVec3d> baseline(
                static_cast<size_t>(geometry->SkinVertexCount));
            for (int32 vertex = 0; vertex < geometry->SkinVertexCount; ++vertex)
                baseline[vertex] = geometry->SkinVertexArray[vertex].SVPoint;
            for (int sample = 1; sample <= 3 && !pose_changes; ++sample) {
                actor.motion_time = actor.motion_start +
                    (actor.motion_end - actor.motion_start) * (sample / 4.0);
                geActor_SetPose(actor.actor, actor.motion,
                                static_cast<geFloat>(actor.motion_time), &transform);
                pose = geActor_GetPoseTransforms(actor.actor);
                geometry = geBodyInst_GetGeometry(actor.body_instance, &actor.scale,
                    pose, GE_BODY_HIGHEST_LOD, nullptr);
                if (!geometry || !geometry->SkinVertexArray ||
                    geometry->SkinVertexCount != static_cast<int32>(baseline.size()))
                    break;
                for (int32 vertex = 0; vertex < geometry->SkinVertexCount; ++vertex) {
                    const geVec3d &before = baseline[vertex];
                    const geVec3d &after = geometry->SkinVertexArray[vertex].SVPoint;
                    const float delta = std::sqrt(
                        (before.X - after.X) * (before.X - after.X) +
                        (before.Y - after.Y) * (before.Y - after.Y) +
                        (before.Z - after.Z) * (before.Z - after.Z));
                    maximum_vertex_delta = (std::max)(maximum_vertex_delta, delta);
                }
                pose_changes = maximum_vertex_delta > 1.0e-4f;
            }
        }
        std::fprintf(stderr,
            "Genesis3D Linux: actor fixture %s: %d vertices, %d faces, "
            "%d normals, %d materials, posed render %s, motions %d, "
            "selected motion %d%s (extent %.3f..%.3f, max vertex delta %.4f)\n",
            paths[fixture].c_str(), vertices, faces, normals,
            geBody_GetMaterialCount(body), valid ? "PASS" : "FAIL",
            actor.motion_count,
            actor.motion_index,
            actor.motion ? (pose_changes ? " (vertex change PASS)" :
                                          " (static motion data)") : "",
            actor.motion_start, actor.motion_end, maximum_vertex_delta);
        destroy_native_actor(&actor);
        if (!valid)
            return false;
    }
    return true;
}

bool render_world_geometry(const geWorld *world,
                           NativeTextureSet &textures,
                           const NativeFaceCache &face_cache,
                           const PlayerCamera &camera,
                           const CameraBasis &basis,
                           NativeActor *native_actor,
                           const NativeLightingState &lighting_state,
                           RenderDiagnostics *diagnostics,
                           int width, int height) {
    if (!world || !world->CurrentBSP || !diagnostics)
        return false;

    const GBSP_BSPData &bsp = world->CurrentBSP->BSPData;
    if (!bsp.GFXModels || bsp.NumGFXModels <= 0 || !bsp.GFXFaces ||
        !bsp.GFXPlanes || !bsp.GFXVerts || !bsp.GFXVertIndexList)
        return false;

    const GFX_Model &root_model = bsp.GFXModels[0];
    const Vec3 minimum = {root_model.Mins.X, root_model.Mins.Y,
                          root_model.Mins.Z};
    const Vec3 maximum = {root_model.Maxs.X, root_model.Maxs.Y,
                          root_model.Maxs.Z};
    const Vec3 extent = subtract(maximum, minimum);
    const double radius = (std::max)(1.0, 0.5 * std::sqrt(
        extent.x * extent.x + extent.y * extent.y + extent.z * extent.z));
    const int safe_width = (std::max)(width, 1);
    const int safe_height = (std::max)(height, 1);
    const double aspect = static_cast<double>(safe_width) /
                          static_cast<double>(safe_height);
    const double near_plane = (std::max)(0.5, radius * 0.001);
    const double far_plane = (std::max)(near_plane + 1.0, radius * 6.0);
    const double half_height = near_plane * std::tan(60.0 * kPi / 360.0);
    const double half_width = half_height * aspect;

    glViewport(0, 0, safe_width, safe_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-half_width, half_width, -half_height, half_height,
              near_plane, far_plane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const Vec3 target = {camera.position.x + basis.forward.x,
                         camera.position.y + basis.forward.y,
                         camera.position.z + basis.forward.z};
    look_at(camera.position, target);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    bool blend_enabled = false;
    GLenum depth_function = GL_LESS;
    bool depth_write = true;
    GLuint bound_material = 0;
    GLuint bound_lightmap = 0;
    const auto set_blend = [&blend_enabled](bool enabled) {
        if (enabled == blend_enabled)
            return;
        enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        blend_enabled = enabled;
    };
    const auto set_depth = [&depth_function, &depth_write](GLenum function,
                                                           bool write) {
        if (function != depth_function) {
            glDepthFunc(function);
            depth_function = function;
        }
        if (write != depth_write) {
            glDepthMask(write ? GL_TRUE : GL_FALSE);
            depth_write = write;
        }
    };
    const auto bind_material = [&bound_material](GLuint texture) {
        if (texture == bound_material)
            return;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        bound_material = texture;
    };
    const auto bind_lightmap = [&bound_lightmap](GLuint texture) {
        if (texture == bound_lightmap)
            return;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture);
        bound_lightmap = texture;
    };

    const int32 first_face = std::max<int32>(0, root_model.FirstFace);
    const int32 final_face = std::min<int32>(
        bsp.NumGFXFaces, first_face + std::max<int32>(0, root_model.NumFaces));
    static std::vector<uint8_t> visible;
    visible.assign(static_cast<size_t>(bsp.NumGFXFaces), 0);
    bool reliable_pvs = false;
    const geVec3d camera_position = {
        static_cast<geFloat>(camera.position.x),
        static_cast<geFloat>(camera.position.y),
        static_cast<geFloat>(camera.position.z)};
    const int32 camera_leaf = Plane_FindLeaf(
        world, root_model.RootNode[0], &camera_position);
    if (camera_leaf >= 0 && camera_leaf < bsp.NumGFXLeafs) {
        const int32 cluster = bsp.GFXLeafs[camera_leaf].Cluster;
        if (cluster >= 0 && cluster < bsp.NumGFXClusters && bsp.GFXVisData &&
            bsp.GFXClusters[cluster].VisOfs >= 0 &&
            bsp.GFXClusters[cluster].VisOfs < bsp.NumGFXVisData) {
            const uint8 *pvs = bsp.GFXVisData + bsp.GFXClusters[cluster].VisOfs;
            reliable_pvs = true;
            for (int32 leaf_index = root_model.FirstLeaf;
                 leaf_index < root_model.FirstLeaf + root_model.NumLeafs &&
                 leaf_index < bsp.NumGFXLeafs; ++leaf_index) {
                const GFX_Leaf &leaf = bsp.GFXLeafs[leaf_index];
                if (leaf.Cluster < 0 || leaf.Cluster >= root_model.NumClusters ||
                    !(pvs[leaf.Cluster >> 3] & (1U << (leaf.Cluster & 7))))
                    continue;
                for (int32 item = 0; item < leaf.NumFaces; ++item) {
                    const int32 list_index = leaf.FirstFace + item;
                    if (list_index < 0 || list_index >= bsp.NumGFXLeafFaces)
                        continue;
                    const int32 face = bsp.GFXLeafFaces[list_index];
                    if (face >= first_face && face < final_face)
                        visible[face] = 1;
                }
            }
        }
    }
    if (!reliable_pvs)
        std::fill(visible.begin() + first_face, visible.begin() + final_face, 1);

    static std::vector<FaceCommand> opaque;
    static std::vector<FaceCommand> translucent;
    opaque.clear();
    translucent.clear();
    opaque.reserve(static_cast<size_t>(final_face - first_face));
    for (int32 face_index = first_face; face_index < final_face; ++face_index) {
        if (!visible[face_index]) {
            ++diagnostics->culled_faces;
            continue;
        }
        const GFX_Face &face = bsp.GFXFaces[face_index];
        double distance_squared = 0.0;
        if (face.NumVerts > 0 && face.FirstVert >= 0 &&
            face.FirstVert < bsp.NumGFXVertIndexList) {
            const int32 vertex_index = bsp.GFXVertIndexList[face.FirstVert];
            if (vertex_index >= 0 && vertex_index < bsp.NumGFXVerts) {
                const geVec3d &point = bsp.GFXVerts[vertex_index];
                const double dx = point.X - camera.position.x;
                const double dy = point.Y - camera.position.y;
                const double dz = point.Z - camera.position.z;
                distance_squared = dx * dx + dy * dy + dz * dz;
            }
        }
        const bool is_translucent = face.TexInfo >= 0 &&
            face.TexInfo < bsp.NumGFXTexInfo &&
            (bsp.GFXTexInfo[face.TexInfo].Flags & TEXINFO_TRANS);
        (is_translucent ? translucent : opaque).push_back(
            {face_index, distance_squared});
    }
    std::sort(translucent.begin(), translucent.end(),
              [](const FaceCommand &left, const FaceCommand &right) {
                  return left.distance_squared > right.distance_squared;
              });
    static std::vector<FaceCommand> commands;
    commands.clear();
    commands.reserve(opaque.size() + translucent.size());
    commands.insert(commands.end(), opaque.begin(), opaque.end());
    commands.insert(commands.end(), translucent.begin(), translucent.end());

    regenerate_visible_lightmaps(world, &textures, lighting_state, commands,
                                 diagnostics);

    diagnostics->visible_faces += commands.size();
    diagnostics->translucent_submissions += translucent.size();
    diagnostics->dynamic_lights += world->LightInfo
        ? static_cast<uint64_t>(world->LightInfo->NumDynamicLights) : 0;
    glBindBuffer(GL_ARRAY_BUFFER, face_cache.vertex_buffer);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(CachedFaceVertex),
                    reinterpret_cast<const void *>(offsetof(CachedFaceVertex, position)));
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(CachedFaceVertex),
                    reinterpret_cast<const void *>(offsetof(CachedFaceVertex, material_uv)));
    glClientActiveTexture(GL_TEXTURE1);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(CachedFaceVertex),
                    reinterpret_cast<const void *>(offsetof(CachedFaceVertex, lightmap_uv)));
    for (const FaceCommand &command : commands) {
        const int32 face_index = command.index;
        const GFX_Face &face = bsp.GFXFaces[face_index];
        if (face.NumVerts < 3 || face.PlaneNum < 0 ||
            face.PlaneNum >= bsp.NumGFXPlanes || face.TexInfo < 0 ||
            face.TexInfo >= bsp.NumGFXTexInfo)
            continue;

        const GFX_TexInfo &texture_info = bsp.GFXTexInfo[face.TexInfo];
        if (texture_info.Texture < 0 ||
            texture_info.Texture >= static_cast<int32>(textures.materials.size()) ||
            texture_info.Texture >= bsp.NumGFXTextures)
            continue;

        const GFX_Texture &texture_metadata =
            bsp.GFXTextures[texture_info.Texture];
        const GLuint material = textures.materials[texture_info.Texture];
        if (!material || texture_metadata.Width <= 0 ||
            texture_metadata.Height <= 0)
            continue;

        const Surf_SurfInfo &surface = world->CurrentBSP->SurfInfo[face_index];
        if (surface.Flags & SURFINFO_LTYPED)
            ++diagnostics->animated_surfaces;
        const float alpha = (texture_info.Flags & TEXINFO_TRANS)
            ? (std::max)(0.0f, (std::min)(255.0f, texture_info.Alpha)) / 255.0f
            : 1.0f;
        set_blend(alpha < 1.0f);
        if (alpha < 1.0f)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        set_depth(GL_LESS, alpha >= 1.0f);
        bind_material(material);
        const GLuint lightmap = textures.lightmaps[face_index];
        bind_lightmap(lightmap && !(texture_info.Flags & TEXINFO_NO_LIGHTMAP)
                          ? lightmap : textures.white_lightmap);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        if (face_index < static_cast<int32>(face_cache.ranges.size())) {
            const NativeFaceCache::Range &range = face_cache.ranges[face_index];
            if (range.count >= 3)
                glDrawArrays(GL_TRIANGLE_FAN, range.first, range.count);
        }
    }

    glClientActiveTexture(GL_TEXTURE1);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    render_native_actor(native_actor, diagnostics);
    return true;
}

int target_fps() {
    const char *value = std::getenv("GENESIS3D_FPS");
    return value && std::atoi(value) >= 120 ? 120 : 60;
}

int window_coordinate(const char *name, int safe_default) {
    const char *value = std::getenv(name);
    return value && *value ? std::atoi(value) : safe_default;
}

bool environment_enabled(const char *name) {
    const char *value = std::getenv(name);
    return value && *value && std::string(value) != "0";
}

std::string project_root() {
    const char *override_root = std::getenv("GENESIS3D_PROJECT_ROOT");
    if (override_root && *override_root)
        return override_root;

    const char *home = std::getenv("HOME");
    return std::string(home && *home ? home : "/home/user") +
           "/Genesis3D_Project/PurificationWorkspace";
}

bool has_extension(const std::string &name,
                   const std::vector<std::string> &extensions) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    for (const std::string &extension : extensions) {
        if (lower_name.size() >= extension.size() &&
            lower_name.compare(lower_name.size() - extension.size(),
                               extension.size(), extension) == 0)
            return true;
    }
    return false;
}

std::string first_asset(const char *kind, const std::string &directory,
                        const char *patterns,
                        const std::vector<std::string> &extensions) {
    DIR *dir = opendir(directory.c_str());
    if (!dir) {
        std::fprintf(stderr,
                     "Genesis3D Linux: missing %s asset directory; searched: %s (%s)\n",
                     kind, directory.c_str(), patterns);
        return {};
    }

    std::vector<std::string> candidates;
    while (dirent *entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (name == "." || name == ".." || !has_extension(name, extensions))
            continue;

        const std::string path = directory + name;
        struct stat attributes {};
        if (stat(path.c_str(), &attributes) == 0 && S_ISREG(attributes.st_mode) &&
            access(path.c_str(), R_OK) == 0)
            candidates.push_back(path);
    }
    closedir(dir);

    if (candidates.empty()) {
        std::fprintf(stderr,
                     "Genesis3D Linux: missing %s asset file; searched: %s (%s)\n",
                     kind, directory.c_str(), patterns);
        return {};
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

std::vector<std::string> neutral_actor_assets(const std::string &directory) {
    std::vector<std::string> candidates;
    DIR *dir = opendir(directory.c_str());
    if (!dir)
        return candidates;
    while (dirent *entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (name.empty() || !std::isupper(static_cast<unsigned char>(name[0])) ||
            !has_extension(name, {".act"}))
            continue;
        candidates.push_back(directory + name);
    }
    closedir(dir);
    std::sort(candidates.begin(), candidates.end());
    return candidates;
}

geWorld *load_world(const std::string &map_path) {
    geVFile *map_file = geVFile_OpenNewSystem(
        nullptr, GE_VFILE_TYPE_DOS, map_path.c_str(), nullptr,
        GE_VFILE_OPEN_READONLY);
    if (!map_file) {
        std::fprintf(stderr,
                     "Genesis3D Linux: could not open level map through geVFile: %s\n",
                     map_path.c_str());
        return nullptr;
    }

    geWorld *world = geWorld_Create(map_file);
    geVFile_Close(map_file);
    if (!world) {
        std::fprintf(stderr,
                     "Genesis3D Linux: Genesis3D could not parse level map: %s\n",
                     map_path.c_str());
        return nullptr;
    }

    std::fprintf(stderr, "Genesis3D Linux: loaded level map: %s\n",
                 map_path.c_str());
    return world;
}
}

int main() {
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    VFileRegistryGuard vfile_registry_guard;
    const int fps = target_fps();
    constexpr double fixed_dt = 1.0 / 60.0;
    const double frame_dt = 1.0 / static_cast<double>(fps);
    const std::string root = project_root();
    const std::string maps_path = root + "/Assets/Maps/";
    const std::string actors_path = root + "/Assets/Actors/";

    /* Resolve and validate resources before graphics initialization. */
    std::fprintf(stderr, "Genesis3D Linux: project root: %s\n", root.c_str());
    const std::string map_file = first_asset(
        "level map", maps_path, "*.bsp or *.gbsp", {".bsp", ".gbsp"});
    std::string actor_file = first_asset(
        "character actor", actors_path, "*.act", {".act"});
    if (map_file.empty() || actor_file.empty())
        return 1;

    std::fprintf(stderr, "Genesis3D Linux: character actor available: %s\n",
                 actor_file.c_str());
    geWorld *world = load_world(map_file);
    if (!world)
        return 1;

    Display *display = XOpenDisplay(nullptr);

    if (!display) {
        std::fprintf(stderr, "Genesis3D Linux: no X11 display; configuration validated (%dx%d, %d FPS, dt=%.9f)\n",
                     kWidth, kHeight, fps, fixed_dt);
        geWorld_Free(world);
        return 0;
    }

    int screen = DefaultScreen(display);
    static int visual_attributes[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
                                      GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                                      GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 24,
                                      None};
    XVisualInfo *visual = glXChooseVisual(display, screen, visual_attributes);
    if (!visual) {
        XCloseDisplay(display);
        std::fprintf(stderr, "Genesis3D Linux: no compatible GLX visual\n");
        geWorld_Free(world);
        return 1;
    }

    Colormap colormap = XCreateColormap(display, RootWindow(display, screen),
                                        visual->visual, AllocNone);
    XSetWindowAttributes attributes{};
    attributes.colormap = colormap;
    attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                            FocusChangeMask | StructureNotifyMask;
    // Fail closed for unattended launches: focus and pointer capture require
    // an explicit interactive opt-in. GENESIS3D_NO_INPUT_CAPTURE always wins.
    const bool input_capture_enabled =
        environment_enabled("GENESIS3D_ENABLE_INPUT_CAPTURE") &&
        !environment_enabled("GENESIS3D_NO_INPUT_CAPTURE");
    const int window_x = window_coordinate("GENESIS3D_WINDOW_X", 892);
    const int window_y = window_coordinate("GENESIS3D_WINDOW_Y", 1080);
    Window window = XCreateWindow(display, RootWindow(display, screen),
                                  window_x, window_y,
                                  kWidth, kHeight, 0, visual->depth,
                                  InputOutput, visual->visual,
                                  CWColormap | CWEventMask, &attributes);
    XStoreName(display, window, "Genesis3D Linux");
    XSizeHints size_hints{};
    size_hints.flags = USPosition;
    size_hints.x = window_x;
    size_hints.y = window_y;
    XSetWMNormalHints(display, window, &size_hints);
    if (!input_capture_enabled) {
        XWMHints wm_hints{};
        wm_hints.flags = InputHint;
        wm_hints.input = False;
        XSetWMHints(display, window, &wm_hints);
    }
    XMapWindow(display, window);
    // Mapping is mediated by the window manager. Wait until it has made this
    // client viewable; merely synchronizing the request queue can still race
    // KWin/Xwayland and make XSetInputFocus fail with BadMatch.
    XEvent map_event{};
    do {
        XWindowEvent(display, window, StructureNotifyMask, &map_event);
    } while (map_event.type != MapNotify);
    if (input_capture_enabled)
        XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XFlush(display);

    GLXContext context = glXCreateContext(display, visual, nullptr, True);
    glXMakeCurrent(display, window, context);
    glViewport(0, 0, kWidth, kHeight);
    glClearColor(0.04f, 0.06f, 0.10f, 1.0f);
    glClearDepth(1.0);

    NativeTextureSet world_textures;
    if (!upload_world_textures(world, &world_textures)) {
        std::fprintf(stderr,
                     "Genesis3D Linux: could not upload world material textures\n");
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }
    NativeFaceCache face_cache;
    if (!build_face_cache(world, &face_cache)) {
        std::fprintf(stderr, "Genesis3D Linux: could not build immutable face cache\n");
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }

    NativeActor native_actor;
    bool actor_loaded = false;
    const std::vector<std::string> actor_fixtures = neutral_actor_assets(actors_path);
    for (const std::string &candidate : actor_fixtures) {
        if (load_native_actor(candidate, &native_actor)) {
            actor_file = candidate;
            actor_loaded = true;
            break;
        }
        destroy_native_actor(&native_actor);
        native_actor = NativeActor{};
    }
    if (!actor_loaded) {
        std::fprintf(stderr,
                     "Genesis3D Linux: actor bridge unavailable; legacy body "
                     "payload could not be parsed: %s\n",
                     actor_file.c_str());
    }

    std::fprintf(stderr,
                 "Genesis3D Linux: textured BSP bridge active (%d root faces, "
                 "%d materials, %d lightmaps; material mean %.1f/255, "
                 "lightmap mean %.1f/255)\n",
                 world->CurrentBSP->BSPData.GFXModels[0].NumFaces,
                 world_textures.uploaded_materials,
                 world_textures.uploaded_lightmaps,
                 world_textures.material_pixel_count
                     ? static_cast<double>(world_textures.material_rgb_sum) /
                           (3.0 * world_textures.material_pixel_count) : 0.0,
                 world_textures.lightmap_pixel_count
                     ? static_cast<double>(world_textures.lightmap_rgb_sum) /
                           (3.0 * world_textures.lightmap_pixel_count) : 0.0);

    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
    SDL_SetHint(SDL_HINT_VIDEO_FOREIGN_WINDOW_OPENGL, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "Genesis3D Linux: SDL2 input initialization failed: %s\n",
                     SDL_GetError());
        destroy_native_actor(&native_actor);
        destroy_face_cache(&face_cache);
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }

    SDL_Window *input_window = SDL_CreateWindowFrom(
        reinterpret_cast<void *>(static_cast<uintptr_t>(window)));
    if (!input_window) {
        std::fprintf(stderr, "Genesis3D Linux: could not attach SDL2 input to X11 window: %s\n",
                     SDL_GetError());
        SDL_Quit();
        destroy_native_actor(&native_actor);
        destroy_face_cache(&face_cache);
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }

    // X11 focus was assigned immediately after mapping, before SDL attached to
    // this foreign window. Synchronize SDL's keyboard-focus bookkeeping now so
    // SDL_GetKeyboardState receives held keys through SDL's X11 event queue.
    if (input_capture_enabled && SDL_SetWindowInputFocus(input_window) != 0)
        std::fprintf(stderr, "Genesis3D Linux: could not synchronize SDL2 input focus: %s\n",
                     SDL_GetError());

    SDL_SetWindowGrab(input_window,
                      input_capture_enabled ? SDL_TRUE : SDL_FALSE);
    if (input_capture_enabled && SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
        std::fprintf(stderr, "Genesis3D Linux: relative mouse mode unavailable: %s\n",
                     SDL_GetError());
    if (!input_capture_enabled)
        std::fprintf(stderr,
                     "Genesis3D Linux: input capture disabled for unattended testing\n");

    PlayerCamera camera = initial_camera(world);
    const CameraBasis actor_basis = camera_basis(camera);
    geXForm3d actor_transform;
    geXForm3d_SetIdentity(&actor_transform);
    actor_transform.Translation.X = static_cast<geFloat>(
        camera.position.x + actor_basis.forward.x * 180.0);
    actor_transform.Translation.Y = static_cast<geFloat>(camera.position.y - 48.0);
    actor_transform.Translation.Z = static_cast<geFloat>(
        camera.position.z + actor_basis.forward.z * 180.0);
    if (native_actor.actor)
        set_actor_root_transform(&native_actor, actor_transform);
    int framebuffer_width = kWidth;
    int framebuffer_height = kHeight;
    constexpr double mouse_sensitivity = 0.0025;
    constexpr double pitch_limit = 89.0 * kPi / 180.0;
    std::fprintf(stderr,
                 "Genesis3D Linux: controls active (WASD/arrows move, mouse looks, Shift sprints, Esc exits)\n");

    // Prime asset-backed command generation before cadence accounting.
    const CameraBasis warmup_basis = camera_basis(camera);
    RenderDiagnostics warmup_diagnostics;
    NativeLightingState lighting_state;
    advance_light_styles(world, &world_textures, &lighting_state, 0.0,
                         &warmup_diagnostics);
    mark_dynamic_lightmaps(world, &world_textures, &lighting_state);
    if (!validate_dynamic_light_cycle(world, &world_textures, &lighting_state)) {
        std::fprintf(stderr,
                     "Genesis3D Linux: dynamic lightmap validation failed\n");
        SDL_DestroyWindow(input_window);
        destroy_native_actor(&native_actor);
        destroy_face_cache(&face_cache);
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        SDL_Quit();
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!render_world_geometry(world, world_textures, face_cache, camera, warmup_basis,
                               &native_actor, lighting_state, &warmup_diagnostics, framebuffer_width,
                               framebuffer_height)) {
        std::fprintf(stderr,
                     "Genesis3D Linux: loaded world has no renderable BSP geometry\n");
        SDL_DestroyWindow(input_window);
        destroy_native_actor(&native_actor);
        destroy_face_cache(&face_cache);
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        SDL_Quit();
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }
    if (!validate_actor_fixtures(actor_fixtures, actor_transform,
                                 &warmup_diagnostics)) {
        std::fprintf(stderr,
                     "Genesis3D Linux: four-asset actor fixture validation failed\n");
        SDL_DestroyWindow(input_window);
        destroy_native_actor(&native_actor);
        destroy_face_cache(&face_cache);
        destroy_world_textures(&world_textures);
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        SDL_Quit();
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XFree(visual);
        XCloseDisplay(display);
        geWorld_Free(world);
        return 1;
    }
    glXSwapBuffers(display, window);
    glFinish();

    using FrameClock = std::chrono::steady_clock;
    const FrameClock::duration frame_period =
        std::chrono::duration_cast<FrameClock::duration>(
            std::chrono::duration<double>(frame_dt));
    FrameClock::time_point previous_tick = FrameClock::now();
    FrameClock::time_point frame_deadline = previous_tick + frame_period;
    const FrameClock::time_point run_started = previous_tick;
    double simulation_accumulator = 0.0;
    uint64_t rendered_frames = 0;
    uint64_t missed_deadlines = 0;
    uint64_t render_deadline_misses = 0;
    uint64_t clamped_elapsed_frames = 0;
    uint64_t maximum_simulation_steps = 0;
    double maximum_render_lateness_ms = 0.0;
    bool running = true;
    RenderDiagnostics render_diagnostics;
    while (running && !stop_requested) {
        const FrameClock::time_point current_tick = FrameClock::now();
        const double elapsed = std::chrono::duration<double>(
            current_tick - previous_tick).count();
        previous_tick = current_tick;
        if (elapsed > 0.25)
            ++clamped_elapsed_frames;
        simulation_accumulator += (std::min)(elapsed, 0.25);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN &&
                 event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                camera.yaw += static_cast<double>(event.motion.xrel) *
                              mouse_sensitivity;
                camera.pitch -= static_cast<double>(event.motion.yrel) *
                                mouse_sensitivity;
                camera.pitch = (std::max)(-pitch_limit,
                                          (std::min)(pitch_limit, camera.pitch));
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    running = false;
                else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    framebuffer_width = (std::max)(event.window.data1, 1);
                    framebuffer_height = (std::max)(event.window.data2, 1);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    if (input_capture_enabled)
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
            }
        }

        const CameraBasis basis = camera_basis(camera);
        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        uint64_t simulation_steps = 0;
        while (simulation_accumulator >= fixed_dt) {
            update_camera_movement(&camera, basis, keys, fixed_dt);
            advance_actor_motion(&native_actor, fixed_dt, &render_diagnostics);
            advance_light_styles(world, &world_textures, &lighting_state,
                                 fixed_dt, &render_diagnostics);
            mark_dynamic_lightmaps(world, &world_textures, &lighting_state);
            simulation_accumulator -= fixed_dt;
            ++simulation_steps;
        }
        maximum_simulation_steps =
            (std::max)(maximum_simulation_steps, simulation_steps);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!render_world_geometry(world, world_textures, face_cache, camera, basis,
                                   &native_actor, lighting_state, &render_diagnostics,
                                   framebuffer_width, framebuffer_height)) {
            std::fprintf(stderr,
                         "Genesis3D Linux: loaded world has no renderable BSP geometry\n");
            running = false;
        }
        glXSwapBuffers(display, window);
        ++rendered_frames;
        const FrameClock::time_point render_finished = FrameClock::now();
        if (render_finished > frame_deadline) {
            ++render_deadline_misses;
            maximum_render_lateness_ms = (std::max)(
                maximum_render_lateness_ms,
                std::chrono::duration<double, std::milli>(
                    render_finished - frame_deadline).count());
        }
        std::this_thread::sleep_until(frame_deadline);
        frame_deadline += frame_period;
        const FrameClock::time_point after_sleep = FrameClock::now();
        if (frame_deadline <= after_sleep) {
            ++missed_deadlines;
            frame_deadline = after_sleep + frame_period;
        }
    }

    const double run_seconds = std::chrono::duration<double>(
        FrameClock::now() - run_started).count();
    std::fprintf(stderr,
                 "Genesis3D Linux: render summary: %llu frames in %.3f s "
                 "(%.2f FPS, %llu schedule resyncs, %llu render deadline "
                 "misses, %.3f ms max lateness, %llu elapsed clamps, %llu "
                 "max fixed steps/frame)\n",
                 static_cast<unsigned long long>(rendered_frames), run_seconds,
                 run_seconds > 0.0 ? rendered_frames / run_seconds : 0.0,
                 static_cast<unsigned long long>(missed_deadlines),
                 static_cast<unsigned long long>(render_deadline_misses),
                 maximum_render_lateness_ms,
                 static_cast<unsigned long long>(clamped_elapsed_frames),
                 static_cast<unsigned long long>(maximum_simulation_steps));

    std::fprintf(stderr,
                 "Genesis3D Linux: pipeline summary: %llu visible faces, "
                 "%llu PVS-culled faces, %llu actor primitives, %llu "
                 "translucent submissions, %llu animated surfaces, %llu "
                 "dynamic lights, %llu active styles, %llu regenerated "
                 "lightmaps (%llu bytes, %.3f ms), %llu actor animation steps\n",
                 static_cast<unsigned long long>(render_diagnostics.visible_faces),
                 static_cast<unsigned long long>(render_diagnostics.culled_faces),
                 static_cast<unsigned long long>(render_diagnostics.actor_submissions),
                 static_cast<unsigned long long>(render_diagnostics.translucent_submissions),
                 static_cast<unsigned long long>(render_diagnostics.animated_surfaces),
                 static_cast<unsigned long long>(render_diagnostics.dynamic_lights),
                 static_cast<unsigned long long>(render_diagnostics.active_light_styles),
                 static_cast<unsigned long long>(render_diagnostics.regenerated_lightmaps),
                 static_cast<unsigned long long>(render_diagnostics.lightmap_upload_bytes),
                 render_diagnostics.lightmap_update_ms,
                 static_cast<unsigned long long>(render_diagnostics.animated_actor_steps));

    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_DestroyWindow(input_window);
    destroy_native_actor(&native_actor);
    destroy_face_cache(&face_cache);
    destroy_world_textures(&world_textures);
    glXMakeCurrent(display, None, nullptr);
    glXDestroyContext(display, context);
    // SDL2-compat's video shutdown releases process-wide GLX display state.
    // Tear down our independently-created GLX context first; reversing these
    // calls makes Mesa's glXDestroyContext access state SDL has already freed.
    SDL_Quit();
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XFree(visual);
    XCloseDisplay(display);
    geWorld_Free(world);
    return 0;
}
