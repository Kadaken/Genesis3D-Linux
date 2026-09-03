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
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "Genesis.h"
#include "Entities/ENTITIES.H"
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

bool render_world_geometry(const geWorld *world,
                           const NativeTextureSet &textures,
                           const PlayerCamera &camera,
                           const CameraBasis &basis,
                           GLuint *geometry_list,
                           int width, int height) {
    if (!world || !world->CurrentBSP || !geometry_list)
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

    if (*geometry_list != 0) {
        glCallList(*geometry_list);
        return true;
    }

    *geometry_list = glGenLists(1);
    if (*geometry_list == 0)
        return false;
    glNewList(*geometry_list, GL_COMPILE);

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
    for (int32 face_index = first_face; face_index < final_face; ++face_index) {
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
        const float draw_scale_u = std::fabs(texture_info.DrawScale[0]) > 1.0e-6f
            ? texture_info.DrawScale[0] : 1.0f;
        const float draw_scale_v = std::fabs(texture_info.DrawScale[1]) > 1.0e-6f
            ? texture_info.DrawScale[1] : 1.0f;
        const float alpha = (texture_info.Flags & TEXINFO_TRANS)
            ? (std::max)(0.0f, (std::min)(255.0f, texture_info.Alpha)) / 255.0f
            : 1.0f;
        set_blend(alpha < 1.0f);
        if (alpha < 1.0f)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        set_depth(GL_LESS, true);
        bind_material(material);
        const GLuint lightmap = textures.lightmaps[face_index];
        bind_lightmap(lightmap && !(texture_info.Flags & TEXINFO_NO_LIGHTMAP)
                          ? lightmap : textures.white_lightmap);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        glBegin(GL_TRIANGLE_FAN);
        for (int32 corner = 0; corner < face.NumVerts; ++corner) {
            const int32 list_index = face.FirstVert + corner;
            if (list_index < 0 || list_index >= bsp.NumGFXVertIndexList)
                continue;
            const int32 vertex_index = bsp.GFXVertIndexList[list_index];
            if (vertex_index < 0 || vertex_index >= bsp.NumGFXVerts)
                continue;
            const geVec3d &vertex = bsp.GFXVerts[vertex_index];
            const Surf_TexVert &texture_vertex =
                world->CurrentBSP->TexVerts[list_index];
            const float texture_u =
                (texture_vertex.u / draw_scale_u + surface.ShiftU) /
                texture_metadata.Width;
            const float texture_v =
                (texture_vertex.v / draw_scale_v + surface.ShiftV) /
                texture_metadata.Height;
            glMultiTexCoord2f(GL_TEXTURE0, texture_u, texture_v);
            const float light_u = lightmap
                ? (texture_vertex.u - surface.LInfo.MinU + 8.0f) /
                      (static_cast<float>(face.LWidth) * 16.0f)
                : 0.5f;
            const float light_v = lightmap
                ? (texture_vertex.v - surface.LInfo.MinV + 8.0f) /
                      (static_cast<float>(face.LHeight) * 16.0f)
                : 0.5f;
            glMultiTexCoord2f(GL_TEXTURE1, light_u, light_v);
            glVertex3f(vertex.X, vertex.Y, vertex.Z);
        }
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEndList();
    glCallList(*geometry_list);
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
    return std::string(home && *home ? home : "/home/user") + "/Genesis3D_Project";
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
    const double fixed_dt = 1.0 / static_cast<double>(fps);
    const std::string root = project_root();
    const std::string maps_path = root + "/Assets/Maps/";
    const std::string actors_path = root + "/Assets/Actors/";

    /* Resolve and validate resources before graphics initialization. */
    std::fprintf(stderr, "Genesis3D Linux: project root: %s\n", root.c_str());
    const std::string map_file = first_asset(
        "level map", maps_path, "*.bsp or *.gbsp", {".bsp", ".gbsp"});
    const std::string actor_file = first_asset(
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
    GLuint world_geometry_list = 0;
    int framebuffer_width = kWidth;
    int framebuffer_height = kHeight;
    constexpr double mouse_sensitivity = 0.0025;
    constexpr double pitch_limit = 89.0 * kPi / 180.0;
    std::fprintf(stderr,
                 "Genesis3D Linux: controls active (WASD/arrows move, mouse looks, Shift sprints, Esc exits)\n");

    // Compile and submit the immutable display list before cadence accounting.
    // This startup work is not a recurring render deadline and otherwise makes
    // a clean steady-state run appear to miss its first several deadlines.
    const CameraBasis warmup_basis = camera_basis(camera);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!render_world_geometry(world, world_textures, camera, warmup_basis,
                               &world_geometry_list, framebuffer_width,
                               framebuffer_height)) {
        std::fprintf(stderr,
                     "Genesis3D Linux: loaded world has no renderable BSP geometry\n");
        SDL_DestroyWindow(input_window);
        SDL_Quit();
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
    glXSwapBuffers(display, window);
    glFinish();

    using FrameClock = std::chrono::steady_clock;
    const FrameClock::duration frame_period =
        std::chrono::duration_cast<FrameClock::duration>(
            std::chrono::duration<double>(fixed_dt));
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
            simulation_accumulator -= fixed_dt;
            ++simulation_steps;
        }
        maximum_simulation_steps =
            (std::max)(maximum_simulation_steps, simulation_steps);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!render_world_geometry(world, world_textures, camera, basis,
                                   &world_geometry_list,
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

    if (world_geometry_list != 0)
        glDeleteLists(world_geometry_list, 1);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_DestroyWindow(input_window);
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
