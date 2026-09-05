/*
 * The contents of this file are subject to the Genesis3D Public License
 * Version 1.01. See ../g3dlicense.txt. Contributor: Kadaken, 2026.
 */

#define GENESIS3D_RENDER_LIBRARY_IMPLEMENTATION 1
#include "../linux_main.cpp"

#include "LinuxRender.h"

#include <climits>
#include <cmath>
#include <cctype>
#include <cstring>
#include <memory>
#include <new>
#include <strings.h>

struct RuntimeActor {
    NativeActor native;
    bool visible = true;
};

struct RuntimeBeam {
    geLinuxRender_Beam beam{};
    double remaining = 0.0;
};

struct RuntimeLight {
    geLight *native = nullptr;
};

struct RuntimeBillboard {
    geLinuxRender_Billboard billboard{};
    double remaining = 0.0;
};

struct geLinuxRender_Runtime {
    geWorld *world = nullptr;
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    Uint32 sdl_owned_subsystems = 0;
    bool headless = false;
    bool input_capture_enabled = false;
    bool close_requested = false;
    int framebuffer_width = 1;
    int framebuffer_height = 1;
    NativeTextureSet world_textures;
    NativeFaceCache face_cache;
    std::vector<std::unique_ptr<RuntimeActor>> actors;
    std::vector<RuntimeBeam> beams;
    std::vector<RuntimeLight> dynamic_lights;
    std::vector<RuntimeBillboard> billboards;
    std::vector<uint8_t> model_visibility;
    NativeLightingState lighting_state;
    RenderDiagnostics diagnostics;
    PlayerCamera camera{{0.0, 0.0, 0.0}, 0.0, 0.0, 400.0};
    PlayerPhysics player_physics;
    HeldMovementInput movement_input;
    bool fire_button = false;
    bool use_requested = false;
    bool screenshot_requested = false;
    bool quick_save_requested = false;
    bool quick_load_requested = false;
    int weapon_slot_requested = -1;
    bool menu_toggle_requested = false;
    bool menu_up_requested = false;
    bool menu_down_requested = false;
    bool menu_left_requested = false;
    bool menu_right_requested = false;
    geLinuxRender_OverlayCallback overlay_callback = nullptr;
    void *overlay_context = nullptr;
};

namespace {

thread_local std::string render_api_error;

void set_api_error(const std::string &message) {
    render_api_error = message;
    std::fprintf(stderr, "Genesis3D render API: %s\n", message.c_str());
}

void clear_runtime_lights(geLinuxRender_Runtime *runtime) {
    if (!runtime)
        return;
    if (runtime->world) {
        for (RuntimeLight &light : runtime->dynamic_lights) {
            if (light.native)
                geWorld_RemoveLight(runtime->world, light.native);
        }
    }
    runtime->dynamic_lights.clear();
}

void release_runtime(geLinuxRender_Runtime *runtime) {
    if (!runtime)
        return;
    if (runtime->context)
        SDL_GL_MakeCurrent(runtime->window, runtime->context);
    for (auto &actor : runtime->actors) {
        if (actor)
            destroy_native_actor(&actor->native);
    }
    runtime->actors.clear();
    clear_runtime_lights(runtime);
    destroy_face_cache(&runtime->face_cache);
    destroy_world_textures(&runtime->world_textures);
    if (runtime->context)
        SDL_GL_DeleteContext(runtime->context);
    if (runtime->window)
        SDL_DestroyWindow(runtime->window);
    if (runtime->sdl_owned_subsystems)
        SDL_QuitSubSystem(runtime->sdl_owned_subsystems);
    if (runtime->world)
        geWorld_Free(runtime->world);
    geVFile_CloseAPI();
    delete runtime;
}

geEntity *entity_at(const geLinuxRender_Runtime *runtime,
                    const char *class_name, size_t entity_index) {
    if (!runtime || !runtime->world || !class_name)
        return nullptr;
    geEntity_EntitySet *set = geWorld_GetEntitySet(runtime->world, class_name);
    if (!set)
        return nullptr;
    geEntity *entity = nullptr;
    for (size_t index = 0; index <= entity_index; ++index) {
        entity = geEntity_EntitySetGetNextEntity(set, entity);
        if (!entity)
            return nullptr;
    }
    return entity;
}

const geEntity_Epair *entity_epair_at(const geEntity *entity,
                                      size_t key_index) {
    if (!entity)
        return nullptr;
    const geEntity_Epair *pair = entity->Epairs;
    for (size_t index = 0; pair && index < key_index; ++index)
        pair = pair->Next;
    return pair;
}

size_t copy_api_string(const char *source, char *buffer, size_t buffer_size) {
    if (!source)
        return 0;
    const size_t required = std::strlen(source) + 1;
    if (buffer && buffer_size > 0) {
        const size_t copied = (std::min)(required - 1, buffer_size - 1);
        std::memcpy(buffer, source, copied);
        buffer[copied] = '\0';
    }
    return required;
}

size_t copy_api_span(const char *source, size_t length, char *buffer,
                     size_t buffer_size) {
    if (!source || length == SIZE_MAX)
        return 0;
    const size_t required = length + 1U;
    if (buffer && buffer_size > 0) {
        const size_t copied = (std::min)(length, buffer_size - 1U);
        std::memcpy(buffer, source, copied);
        buffer[copied] = '\0';
    }
    return required;
}

int material_index(const geLinuxRender_Runtime *runtime, const char *name) {
    if (!runtime || !runtime->world || !runtime->world->CurrentBSP ||
        !name || !*name)
        return -1;
    const size_t requested_length = strnlen(name, 65U);
    if (requested_length == 65U)
        return -1;
    const GBSP_BSPData &bsp = runtime->world->CurrentBSP->BSPData;
    for (int32 index = 0; index < bsp.NumGFXTextures; ++index) {
        const GFX_Texture &material = bsp.GFXTextures[index];
        const size_t length = strnlen(material.Name, sizeof(material.Name));
        if (requested_length == length &&
            strncasecmp(material.Name, name, length) == 0)
            return index;
    }
    return -1;
}

geLinuxRender_Camera export_camera(const PlayerCamera &camera) {
    return {{camera.position.x, camera.position.y, camera.position.z},
            camera.yaw, camera.pitch, camera.move_speed};
}

PlayerCamera import_camera(const geLinuxRender_Camera &camera) {
    return {{camera.Position.X, camera.Position.Y, camera.Position.Z},
            camera.YawRadians, camera.PitchRadians, camera.MoveSpeed};
}

bool valid_camera(const geLinuxRender_Camera &camera) {
    return std::isfinite(camera.Position.X) &&
           std::isfinite(camera.Position.Y) &&
           std::isfinite(camera.Position.Z) &&
           std::isfinite(camera.YawRadians) &&
           std::isfinite(camera.PitchRadians) &&
           std::isfinite(camera.MoveSpeed) && camera.MoveSpeed > 0.0;
}

bool valid_vector(const geLinuxRender_Vec3 &point) {
    return std::isfinite(point.X) && std::isfinite(point.Y) &&
           std::isfinite(point.Z);
}

bool valid_beam(const geLinuxRender_Beam &beam) {
    return valid_vector(beam.Start) && valid_vector(beam.End) &&
           std::isfinite(beam.Red) && std::isfinite(beam.Green) &&
           std::isfinite(beam.Blue) && std::isfinite(beam.Alpha) &&
           std::isfinite(beam.Width) &&
           std::isfinite(beam.LifetimeSeconds) && beam.Width > 0.0f &&
           beam.Red >= 0.0f && beam.Red <= 1.0f &&
           beam.Green >= 0.0f && beam.Green <= 1.0f &&
           beam.Blue >= 0.0f && beam.Blue <= 1.0f &&
           beam.Alpha >= 0.0f && beam.Alpha <= 1.0f &&
           beam.Width <= 1024.0f && beam.LifetimeSeconds > 0.0 &&
           beam.LifetimeSeconds <= 60.0;
}

bool valid_light(const geLinuxRender_Vec3 &position,
                 const geLinuxRender_Color3 &color, double radius) {
    return valid_vector(position) && std::isfinite(color.Red) &&
           std::isfinite(color.Green) && std::isfinite(color.Blue) &&
           color.Red >= 0.0f && color.Red <= 255.0f &&
           color.Green >= 0.0f && color.Green <= 255.0f &&
           color.Blue >= 0.0f && color.Blue <= 255.0f &&
           std::isfinite(radius) && radius > 0.0 && radius <= 65536.0;
}

bool valid_billboard(const geLinuxRender_Billboard &billboard) {
    return valid_vector(billboard.Position) &&
           std::isfinite(billboard.Red) &&
           std::isfinite(billboard.Green) &&
           std::isfinite(billboard.Blue) &&
           std::isfinite(billboard.Alpha) &&
           std::isfinite(billboard.Size) &&
           std::isfinite(billboard.LifetimeSeconds) &&
           billboard.Red >= 0.0f && billboard.Red <= 1.0f &&
           billboard.Green >= 0.0f && billboard.Green <= 1.0f &&
           billboard.Blue >= 0.0f && billboard.Blue <= 1.0f &&
           billboard.Alpha >= 0.0f && billboard.Alpha <= 1.0f &&
           billboard.Size > 0.0f && billboard.Size <= 4096.0f &&
           billboard.LifetimeSeconds > 0.0 &&
           billboard.LifetimeSeconds <= 60.0 &&
           (billboard.Additive == 0 || billboard.Additive == 1);
}

int set_runtime_light(geLinuxRender_Runtime *runtime, RuntimeLight &light,
                      const geLinuxRender_Vec3 &position,
                      const geLinuxRender_Color3 &color, double radius) {
    if (!runtime || !runtime->world || !light.native ||
        !valid_light(position, color, radius))
        return 0;
    const geVec3d native_position{
        static_cast<geFloat>(position.X),
        static_cast<geFloat>(position.Y),
        static_cast<geFloat>(position.Z)};
    const GE_RGBA native_color{color.Red, color.Green, color.Blue, 255.0f};
    if (!geWorld_SetLightAttributes(runtime->world, light.native,
                                    &native_position, &native_color,
                                    static_cast<geFloat>(radius), GE_FALSE))
        return 0;
    if (!runtime->headless) {
        mark_dynamic_lightmaps(runtime->world, &runtime->world_textures,
                               &runtime->lighting_state);
    }
    return 1;
}

void render_runtime_beams(const geLinuxRender_Runtime *runtime,
                          const CameraBasis &basis) {
    if (!runtime || runtime->beams.empty())
        return;
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_TEXTURE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    for (const RuntimeBeam &runtime_beam : runtime->beams) {
        const geLinuxRender_Beam &beam = runtime_beam.beam;
        const double half_width = static_cast<double>(beam.Width) * 0.5;
        const double rx = basis.right.x * half_width;
        const double ry = basis.right.y * half_width;
        const double rz = basis.right.z * half_width;
        glColor4f(beam.Red, beam.Green, beam.Blue, beam.Alpha);
        glVertex3d(beam.Start.X - rx, beam.Start.Y - ry, beam.Start.Z - rz);
        glVertex3d(beam.Start.X + rx, beam.Start.Y + ry, beam.Start.Z + rz);
        glVertex3d(beam.End.X + rx, beam.End.Y + ry, beam.End.Z + rz);
        glVertex3d(beam.End.X - rx, beam.End.Y - ry, beam.End.Z - rz);
    }
    glEnd();
    glPopAttrib();
}

void render_runtime_billboards(const geLinuxRender_Runtime *runtime,
                               const CameraBasis &basis) {
    if (!runtime || runtime->billboards.empty())
        return;
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_TEXTURE_BIT | GL_CURRENT_BIT);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    for (int additive = 0; additive <= 1; ++additive) {
        glBlendFunc(GL_SRC_ALPHA,
                    additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        for (const RuntimeBillboard &runtime_billboard :
             runtime->billboards) {
            const geLinuxRender_Billboard &billboard =
                runtime_billboard.billboard;
            if (billboard.Additive != additive)
                continue;
            const double half = static_cast<double>(billboard.Size) * 0.5;
            const double rx = basis.right.x * half;
            const double ry = basis.right.y * half;
            const double rz = basis.right.z * half;
            const double ux = basis.up.x * half;
            const double uy = basis.up.y * half;
            const double uz = basis.up.z * half;
            glColor4f(billboard.Red, billboard.Green, billboard.Blue,
                      billboard.Alpha);
            glVertex3d(billboard.Position.X - rx - ux,
                       billboard.Position.Y - ry - uy,
                       billboard.Position.Z - rz - uz);
            glVertex3d(billboard.Position.X + rx - ux,
                       billboard.Position.Y + ry - uy,
                       billboard.Position.Z + rz - uz);
            glVertex3d(billboard.Position.X + rx + ux,
                       billboard.Position.Y + ry + uy,
                       billboard.Position.Z + rz + uz);
            glVertex3d(billboard.Position.X - rx + ux,
                       billboard.Position.Y - ry + uy,
                       billboard.Position.Z - rz + uz);
        }
        glEnd();
    }
    glPopAttrib();
}

std::string normalized_motion_name(const char *name) {
    if (!name)
        return {};
    std::string result(name);
    const size_t separator = result.find_last_of("/\\");
    if (separator != std::string::npos)
        result.erase(0, separator + 1);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    const size_t extension = result.rfind('.');
    if (extension != std::string::npos)
        result.erase(extension);
    return result;
}

void export_collision(const GE_Collision &collision,
                      geLinuxRender_TraceResult *result) {
    result->Hit = 1;
    result->Kind = collision.Actor ? GE_LINUX_RENDER_HIT_ACTOR
                   : collision.Mesh ? GE_LINUX_RENDER_HIT_MESH
                   : GE_LINUX_RENDER_HIT_WORLD_MODEL;
    result->Impact = {collision.Impact.X, collision.Impact.Y,
                      collision.Impact.Z};
    result->Normal = {collision.Plane.Normal.X, collision.Plane.Normal.Y,
                      collision.Plane.Normal.Z};
    result->Fraction = (std::max)(0.0, (std::min)(1.0,
        static_cast<double>(collision.Ratio)));
}

bool load_runtime_actor(geLinuxRender_Runtime *runtime,
                        const char *actor_directory,
                        bool upload_materials) {
    if (!runtime || !actor_directory || !*actor_directory)
        return false;
    std::string directory(actor_directory);
    if (directory.back() != '/')
        directory.push_back('/');
    for (const std::string &candidate : neutral_actor_assets(directory)) {
        std::unique_ptr<RuntimeActor> actor(
            new (std::nothrow) RuntimeActor());
        if (!actor)
            return false;
        if (load_native_actor(candidate, &actor->native, upload_materials)) {
            runtime->actors.push_back(std::move(actor));
            return true;
        }
        destroy_native_actor(&actor->native);
    }
    return false;
}

void place_runtime_actor_at_default(geLinuxRender_Runtime *runtime) {
    if (!runtime || runtime->actors.empty() || !runtime->actors[0] ||
        !runtime->actors[0]->native.actor)
        return;
    const CameraBasis basis = camera_basis(runtime->camera);
    geXForm3d transform;
    geXForm3d_SetIdentity(&transform);
    transform.Translation.X = static_cast<geFloat>(
        runtime->camera.position.x + basis.forward.x * 180.0);
    transform.Translation.Y = static_cast<geFloat>(
        runtime->camera.position.y - 48.0);
    transform.Translation.Z = static_cast<geFloat>(
        runtime->camera.position.z + basis.forward.z * 180.0);
    set_actor_root_transform(&runtime->actors[0]->native, transform);
}

RuntimeActor *runtime_actor_at(geLinuxRender_Runtime *runtime,
                               size_t actor_index) {
    return runtime && actor_index < runtime->actors.size()
        ? runtime->actors[actor_index].get() : nullptr;
}

const RuntimeActor *runtime_actor_at(const geLinuxRender_Runtime *runtime,
                                     size_t actor_index) {
    return runtime && actor_index < runtime->actors.size()
        ? runtime->actors[actor_index].get() : nullptr;
}

} // namespace

extern "C" geLinuxRender_Runtime *
geLinuxRender_Create(const geLinuxRender_Config *config) {
    render_api_error.clear();
    if (!config || config->StructSize < sizeof(geLinuxRender_Config) ||
        config->ApiVersion != GE_LINUX_RENDER_API_VERSION ||
        !config->MapPath || !*config->MapPath) {
        set_api_error("invalid configuration or API version");
        return nullptr;
    }

    geLinuxRender_Runtime *runtime = new (std::nothrow) geLinuxRender_Runtime;
    if (!runtime) {
        set_api_error("out of memory creating renderer runtime");
        return nullptr;
    }
    runtime->headless = config->Headless != 0;
    runtime->input_capture_enabled = config->EnableInputCapture != 0 &&
                                     !runtime->headless;
    runtime->world = load_world(config->MapPath);
    if (!runtime->world) {
        set_api_error("Genesis3D could not load the configured map");
        release_runtime(runtime);
        return nullptr;
    }
    runtime->camera = initial_camera(runtime->world);
    runtime->model_visibility.assign(
        geLinuxRender_GetWorldModelCount(runtime), 1U);
    if (runtime->headless) {
        load_runtime_actor(runtime, config->ActorDirectory, false);
        place_runtime_actor_at_default(runtime);
        return runtime;
    }

    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
    constexpr Uint32 requested_subsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    const Uint32 existing_subsystems = SDL_WasInit(requested_subsystems);
    const Uint32 missing_subsystems = requested_subsystems & ~existing_subsystems;
    if (missing_subsystems && SDL_InitSubSystem(missing_subsystems) != 0) {
        set_api_error(std::string("SDL initialization failed: ") + SDL_GetError());
        release_runtime(runtime);
        return nullptr;
    }
    runtime->sdl_owned_subsystems = missing_subsystems;
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    const int width = config->Width > 0 ? config->Width : kWidth;
    const int height = config->Height > 0 ? config->Height : kHeight;
    const char *title = config->WindowTitle && *config->WindowTitle
                            ? config->WindowTitle : "Genesis3D Runtime";
    runtime->window = SDL_CreateWindow(
        title, config->WindowX, config->WindowY, width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (!runtime->window) {
        set_api_error(std::string("SDL window creation failed: ") + SDL_GetError());
        release_runtime(runtime);
        return nullptr;
    }

    if (!runtime->input_capture_enabled) {
        SDL_SysWMinfo wm_info{};
        SDL_VERSION(&wm_info.version);
        if (SDL_GetWindowWMInfo(runtime->window, &wm_info) == SDL_TRUE &&
            wm_info.subsystem == SDL_SYSWM_X11) {
            XWMHints wm_hints{};
            wm_hints.flags = InputHint;
            wm_hints.input = False;
            XSetWMHints(wm_info.info.x11.display, wm_info.info.x11.window,
                        &wm_hints);
            XFlush(wm_info.info.x11.display);
        }
    }
    if (config->ShowWindow)
        SDL_ShowWindow(runtime->window);
    if (runtime->input_capture_enabled) {
        SDL_SetWindowGrab(runtime->window, SDL_TRUE);
        if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
            std::fprintf(stderr, "Genesis3D render API: relative mouse mode unavailable: %s\n",
                         SDL_GetError());
    }

    runtime->context = SDL_GL_CreateContext(runtime->window);
    if (!runtime->context ||
        SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("OpenGL context creation failed: ") + SDL_GetError());
        release_runtime(runtime);
        return nullptr;
    }
    SDL_GL_SetSwapInterval(0);
    SDL_GL_GetDrawableSize(runtime->window, &runtime->framebuffer_width,
                           &runtime->framebuffer_height);
    runtime->framebuffer_width = (std::max)(runtime->framebuffer_width, 1);
    runtime->framebuffer_height = (std::max)(runtime->framebuffer_height, 1);
    glViewport(0, 0, runtime->framebuffer_width, runtime->framebuffer_height);
    glClearColor(0.04f, 0.06f, 0.10f, 1.0f);
    glClearDepth(1.0);

    if (!upload_world_textures(runtime->world, &runtime->world_textures) ||
        !build_face_cache(runtime->world, &runtime->face_cache)) {
        set_api_error("could not initialize world texture/geometry caches");
        release_runtime(runtime);
        return nullptr;
    }

    load_runtime_actor(runtime, config->ActorDirectory, true);
    place_runtime_actor_at_default(runtime);

    advance_light_styles(runtime->world, &runtime->world_textures,
                         &runtime->lighting_state, 0.0, &runtime->diagnostics);
    mark_dynamic_lightmaps(runtime->world, &runtime->world_textures,
                           &runtime->lighting_state);
    std::fprintf(stderr,
                 "Genesis3D render API: initialized in-process runtime (%s)\n",
                 runtime->input_capture_enabled ? "interactive" : "no input capture");
    return runtime;
}

extern "C" void geLinuxRender_Destroy(geLinuxRender_Runtime *runtime) {
    if (runtime && runtime->input_capture_enabled)
        SDL_SetRelativeMouseMode(SDL_FALSE);
    release_runtime(runtime);
}

extern "C" int geLinuxRender_LoadMap(geLinuxRender_Runtime *runtime,
                                      const char *map_path) {
    render_api_error.clear();
    if (!runtime || !map_path || !*map_path) {
        set_api_error("invalid map reload request");
        return 0;
    }

    geWorld *replacement_world = load_world(map_path);
    if (!replacement_world) {
        set_api_error("Genesis3D could not load the replacement map");
        return 0;
    }

    NativeTextureSet replacement_textures;
    NativeFaceCache replacement_cache;
    if (!runtime->headless) {
        if (SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
            set_api_error(std::string("could not activate OpenGL context for map reload: ") +
                          SDL_GetError());
            geWorld_Free(replacement_world);
            return 0;
        }
        if (!upload_world_textures(replacement_world, &replacement_textures) ||
            !build_face_cache(replacement_world, &replacement_cache)) {
            destroy_face_cache(&replacement_cache);
            destroy_world_textures(&replacement_textures);
            geWorld_Free(replacement_world);
            set_api_error("could not initialize replacement world caches");
            return 0;
        }
    }

    clear_runtime_lights(runtime);
    destroy_face_cache(&runtime->face_cache);
    destroy_world_textures(&runtime->world_textures);
    geWorld_Free(runtime->world);
    runtime->world = replacement_world;
    runtime->world_textures = std::move(replacement_textures);
    runtime->face_cache = std::move(replacement_cache);
    runtime->lighting_state = NativeLightingState{};
    runtime->diagnostics = RenderDiagnostics{};
    runtime->beams.clear();
    runtime->billboards.clear();
    runtime->player_physics = PlayerPhysics{};
    runtime->movement_input = HeldMovementInput{};
    runtime->fire_button = false;
    runtime->use_requested = false;
    runtime->screenshot_requested = false;
    runtime->quick_save_requested = false;
    runtime->quick_load_requested = false;
    runtime->weapon_slot_requested = -1;
    runtime->menu_toggle_requested = false;
    runtime->menu_up_requested = false;
    runtime->menu_down_requested = false;
    runtime->menu_left_requested = false;
    runtime->menu_right_requested = false;
    runtime->camera = initial_camera(runtime->world);
    runtime->model_visibility.assign(
        geLinuxRender_GetWorldModelCount(runtime), 1U);
    place_runtime_actor_at_default(runtime);
    if (!runtime->headless) {
        advance_light_styles(runtime->world, &runtime->world_textures,
                             &runtime->lighting_state, 0.0,
                             &runtime->diagnostics);
        mark_dynamic_lightmaps(runtime->world, &runtime->world_textures,
                               &runtime->lighting_state);
    }
    std::fprintf(stderr,
                 "Genesis3D render API: replacement map committed atomically\n");
    return 1;
}

extern "C" int geLinuxRender_PollInput(geLinuxRender_Runtime *runtime,
                                        geLinuxRender_Input *input) {
    if (!runtime || !input)
        return 0;
    *input = geLinuxRender_Input{};
    input->WeaponSlot = -1;
    if (runtime->headless)
        return 1;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_KEYDOWN &&
             event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
            runtime->close_requested = true;
        } else if (event.type == SDL_MOUSEMOTION) {
            input->LookDeltaX += static_cast<double>(event.motion.xrel);
            input->LookDeltaY += static_cast<double>(event.motion.yrel);
        } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                   event.button.button == SDL_BUTTON_LEFT) {
            runtime->fire_button = true;
        } else if (event.type == SDL_MOUSEBUTTONUP &&
                   event.button.button == SDL_BUTTON_LEFT) {
            runtime->fire_button = false;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode == SDL_SCANCODE_E)
                runtime->use_requested = true;
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode == SDL_SCANCODE_F12)
                runtime->screenshot_requested = true;
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode == SDL_SCANCODE_F9)
                runtime->quick_save_requested = true;
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode == SDL_SCANCODE_F10)
                runtime->quick_load_requested = true;
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode == SDL_SCANCODE_F2)
                runtime->menu_toggle_requested = true;
            if (event.key.keysym.scancode == SDL_SCANCODE_UP)
                runtime->menu_up_requested = true;
            if (event.key.keysym.scancode == SDL_SCANCODE_DOWN)
                runtime->menu_down_requested = true;
            if (event.key.keysym.scancode == SDL_SCANCODE_LEFT)
                runtime->menu_left_requested = true;
            if (event.key.keysym.scancode == SDL_SCANCODE_RIGHT)
                runtime->menu_right_requested = true;
            if (event.key.repeat == 0 &&
                event.key.keysym.scancode >= SDL_SCANCODE_1 &&
                event.key.keysym.scancode <= SDL_SCANCODE_9) {
                runtime->weapon_slot_requested =
                    static_cast<int>(event.key.keysym.scancode -
                                     SDL_SCANCODE_1);
            }
            set_movement_key(&runtime->movement_input,
                             event.key.keysym.scancode, true);
        } else if (event.type == SDL_KEYUP) {
            set_movement_key(&runtime->movement_input,
                             event.key.keysym.scancode, false);
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                runtime->close_requested = true;
            } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                SDL_GL_GetDrawableSize(runtime->window,
                                       &runtime->framebuffer_width,
                                       &runtime->framebuffer_height);
                runtime->framebuffer_width = (std::max)(runtime->framebuffer_width, 1);
                runtime->framebuffer_height = (std::max)(runtime->framebuffer_height, 1);
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED &&
                       runtime->input_capture_enabled) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                SDL_SetRelativeMouseMode(SDL_FALSE);
                runtime->movement_input = HeldMovementInput{};
                runtime->fire_button = false;
            }
        }
    }

    const HeldMovementInput &held = runtime->movement_input;
    input->MoveForward = (held.forward_w || held.forward_up ? 1.0 : 0.0) -
                         (held.backward_s || held.backward_down ? 1.0 : 0.0);
    input->MoveRight = (held.right_d || held.right_arrow ? 1.0 : 0.0) -
                       (held.left_a || held.left_arrow ? 1.0 : 0.0);
    input->Sprint = held.sprint_left || held.sprint_right;
    input->Jump = held.jump_space;
    input->Fire = runtime->fire_button;
    input->Use = runtime->use_requested ? 1 : 0;
    input->Screenshot = runtime->screenshot_requested ? 1 : 0;
    input->QuickSave = runtime->quick_save_requested ? 1 : 0;
    input->QuickLoad = runtime->quick_load_requested ? 1 : 0;
    input->WeaponSlot = runtime->weapon_slot_requested;
    input->MenuToggle = runtime->menu_toggle_requested ? 1 : 0;
    input->MenuUp = runtime->menu_up_requested ? 1 : 0;
    input->MenuDown = runtime->menu_down_requested ? 1 : 0;
    input->MenuLeft = runtime->menu_left_requested ? 1 : 0;
    input->MenuRight = runtime->menu_right_requested ? 1 : 0;
    runtime->use_requested = false;
    runtime->screenshot_requested = false;
    runtime->quick_save_requested = false;
    runtime->quick_load_requested = false;
    runtime->weapon_slot_requested = -1;
    runtime->menu_toggle_requested = false;
    runtime->menu_up_requested = false;
    runtime->menu_down_requested = false;
    runtime->menu_left_requested = false;
    runtime->menu_right_requested = false;
    input->QuitRequested = runtime->close_requested;
    return 1;
}

extern "C" int geLinuxRender_SetCamera(geLinuxRender_Runtime *runtime,
                                        const geLinuxRender_Camera *camera) {
    if (!runtime || !camera || !valid_camera(*camera))
        return 0;
    runtime->camera = import_camera(*camera);
    constexpr double pitch_limit = 89.0 * kPi / 180.0;
    runtime->camera.pitch = (std::max)(
        -pitch_limit, (std::min)(pitch_limit, runtime->camera.pitch));
    return 1;
}

extern "C" int geLinuxRender_GetInitialCamera(
    const geLinuxRender_Runtime *runtime, geLinuxRender_Camera *camera) {
    if (!runtime || !camera)
        return 0;
    *camera = export_camera(runtime->camera);
    return 1;
}

extern "C" int geLinuxRender_StepPlayer(
    geLinuxRender_Runtime *runtime, const geLinuxRender_Input *input,
    double fixed_delta_seconds, geLinuxRender_Camera *camera) {
    if (!runtime || !input || !camera || !valid_camera(*camera) ||
        !std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
        return 0;
    PlayerCamera native_camera = import_camera(*camera);
    HeldMovementInput held;
    held.forward_w = input->MoveForward > 0.0;
    held.backward_s = input->MoveForward < 0.0;
    held.right_d = input->MoveRight > 0.0;
    held.left_a = input->MoveRight < 0.0;
    held.sprint_left = input->Sprint != 0;
    held.jump_space = input->Jump != 0;
    update_player_movement(runtime->world, &native_camera,
                           &runtime->player_physics,
                           camera_basis(native_camera), held,
                           fixed_delta_seconds);
    *camera = export_camera(native_camera);
    return 1;
}

extern "C" int geLinuxRender_SetPlayerMedium(
    geLinuxRender_Runtime *runtime,
    const geLinuxRender_PlayerMedium *medium) {
    if (!runtime || !medium || !std::isfinite(medium->SpeedScale) ||
        !std::isfinite(medium->GravityScale) ||
        !std::isfinite(medium->SwimAcceleration) ||
        medium->SpeedScale < 0.05 || medium->SpeedScale > 4.0 ||
        medium->GravityScale < 0.0 || medium->GravityScale > 4.0 ||
        medium->SwimAcceleration < 0.0 ||
        medium->SwimAcceleration > 4000.0)
        return 0;
    runtime->player_physics.speed_scale = medium->SpeedScale;
    runtime->player_physics.gravity_scale = medium->GravityScale;
    runtime->player_physics.swim_acceleration = medium->SwimAcceleration;
    return 1;
}

extern "C" int geLinuxRender_SetPlayerMovement(
    geLinuxRender_Runtime *runtime,
    const geLinuxRender_PlayerMovement *movement) {
    if (!runtime || !movement || !std::isfinite(movement->Gravity) ||
        !std::isfinite(movement->JumpSpeed) ||
        !std::isfinite(movement->StepHeight) ||
        movement->Gravity < 1.0 || movement->Gravity > 10000.0 ||
        movement->JumpSpeed < 1.0 || movement->JumpSpeed > 5000.0 ||
        movement->StepHeight < 0.1 || movement->StepHeight > 256.0)
        return 0;
    runtime->player_physics.gravity = movement->Gravity;
    runtime->player_physics.jump_speed = movement->JumpSpeed;
    runtime->player_physics.step_height = movement->StepHeight;
    return 1;
}

extern "C" int geLinuxRender_AdvanceSimulation(
    geLinuxRender_Runtime *runtime, double fixed_delta_seconds) {
    if (!runtime || !std::isfinite(fixed_delta_seconds) ||
        fixed_delta_seconds < 0.0)
        return 0;
    for (RuntimeBeam &beam : runtime->beams)
        beam.remaining -= fixed_delta_seconds;
    runtime->beams.erase(
        std::remove_if(runtime->beams.begin(), runtime->beams.end(),
                       [](const RuntimeBeam &beam) {
                           return beam.remaining <= 0.0;
                       }),
        runtime->beams.end());
    for (RuntimeBillboard &billboard : runtime->billboards)
        billboard.remaining -= fixed_delta_seconds;
    runtime->billboards.erase(
        std::remove_if(runtime->billboards.begin(),
                       runtime->billboards.end(),
                       [](const RuntimeBillboard &billboard) {
                           return billboard.remaining <= 0.0;
                       }),
        runtime->billboards.end());
    if (!runtime->headless) {
        for (auto &actor : runtime->actors) {
            if (actor)
                advance_actor_motion(&actor->native, fixed_delta_seconds,
                                     &runtime->diagnostics);
        }
        advance_light_styles(runtime->world, &runtime->world_textures,
                             &runtime->lighting_state, fixed_delta_seconds,
                             &runtime->diagnostics);
        mark_dynamic_lightmaps(runtime->world, &runtime->world_textures,
                               &runtime->lighting_state);
    }
    return 1;
}

extern "C" int geLinuxRender_ResetPlayerPhysics(
    geLinuxRender_Runtime *runtime) {
    if (!runtime)
        return 0;
    const PlayerPhysics configured = runtime->player_physics;
    runtime->player_physics = PlayerPhysics{};
    runtime->player_physics.speed_scale = configured.speed_scale;
    runtime->player_physics.gravity_scale = configured.gravity_scale;
    runtime->player_physics.swim_acceleration = configured.swim_acceleration;
    runtime->player_physics.gravity = configured.gravity;
    runtime->player_physics.jump_speed = configured.jump_speed;
    runtime->player_physics.step_height = configured.step_height;
    runtime->movement_input = HeldMovementInput{};
    runtime->fire_button = false;
    return 1;
}

extern "C" int geLinuxRender_RenderFrame(geLinuxRender_Runtime *runtime) {
    if (!runtime || runtime->headless || runtime->close_requested)
        return 0;
    if (SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("could not activate OpenGL context: ") + SDL_GetError());
        return 0;
    }
    glViewport(0, 0, runtime->framebuffer_width, runtime->framebuffer_height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const CameraBasis basis = camera_basis(runtime->camera);
    RuntimeActor *primary = runtime_actor_at(runtime, 0);
    NativeActor *primary_native = primary && primary->visible
        ? &primary->native : nullptr;
    if (!render_world_geometry(runtime->world, runtime->world_textures,
                               runtime->face_cache, runtime->camera, basis,
                               primary_native,
                               runtime->lighting_state,
                               &runtime->diagnostics,
                               runtime->framebuffer_width,
                               runtime->framebuffer_height,
                               &runtime->model_visibility)) {
        set_api_error("world has no renderable BSP geometry");
        return 0;
    }
    for (size_t index = 1; index < runtime->actors.size(); ++index) {
        RuntimeActor *actor = runtime_actor_at(runtime, index);
        if (actor && actor->visible)
            render_native_actor(&actor->native, &runtime->diagnostics);
    }
    render_runtime_beams(runtime, basis);
    render_runtime_billboards(runtime, basis);
    if (runtime->overlay_callback)
        runtime->overlay_callback(runtime->overlay_context,
                                  runtime->framebuffer_width,
                                  runtime->framebuffer_height);
    SDL_GL_SwapWindow(runtime->window);
    return 1;
}

extern "C" int geLinuxRender_SaveScreenshot(
    geLinuxRender_Runtime *runtime, const char *path) {
    if (!runtime || runtime->headless || !runtime->window ||
        !runtime->context || !path || !*path ||
        runtime->framebuffer_width <= 0 || runtime->framebuffer_height <= 0)
        return 0;
    if (SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("could not activate screenshot context: ") +
                      SDL_GetError());
        return 0;
    }
    const size_t width = static_cast<size_t>(runtime->framebuffer_width);
    const size_t height = static_cast<size_t>(runtime->framebuffer_height);
    if (width > SIZE_MAX / 3U || height > SIZE_MAX / (width * 3U) ||
        width > static_cast<size_t>(INT_MAX) / 3U) {
        set_api_error("screenshot dimensions overflow the pixel buffer");
        return 0;
    }
    std::vector<Uint8> pixels(width * height * 3U);
    GLint previous_pack_alignment = 0;
    GLint previous_read_buffer = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);
    while (glGetError() != GL_NO_ERROR) {}
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glFinish();
    glReadPixels(0, 0, runtime->framebuffer_width,
                 runtime->framebuffer_height, GL_RGB, GL_UNSIGNED_BYTE,
                 pixels.data());
    const GLenum read_error = glGetError();
    glReadBuffer(static_cast<GLenum>(previous_read_buffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
    if (read_error != GL_NO_ERROR) {
        set_api_error("OpenGL framebuffer read failed");
        return 0;
    }
    const size_t row_bytes = width * 3U;
    std::vector<Uint8> row(row_bytes);
    for (size_t top = 0; top < height / 2U; ++top) {
        Uint8 *upper = pixels.data() + top * row_bytes;
        Uint8 *lower = pixels.data() + (height - top - 1U) * row_bytes;
        std::memcpy(row.data(), upper, row_bytes);
        std::memcpy(upper, lower, row_bytes);
        std::memcpy(lower, row.data(), row_bytes);
    }
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    constexpr Uint32 red_mask = 0xff0000U;
    constexpr Uint32 green_mask = 0x00ff00U;
    constexpr Uint32 blue_mask = 0x0000ffU;
#else
    constexpr Uint32 red_mask = 0x0000ffU;
    constexpr Uint32 green_mask = 0x00ff00U;
    constexpr Uint32 blue_mask = 0xff0000U;
#endif
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        pixels.data(), runtime->framebuffer_width,
        runtime->framebuffer_height, 24,
        static_cast<int>(row_bytes), red_mask, green_mask, blue_mask, 0U);
    if (!surface) {
        set_api_error(std::string("could not create screenshot surface: ") +
                      SDL_GetError());
        return 0;
    }
    const int saved = SDL_SaveBMP(surface, path);
    SDL_FreeSurface(surface);
    if (saved != 0) {
        set_api_error(std::string("could not save screenshot: ") +
                      SDL_GetError());
        return 0;
    }
    return 1;
}

extern "C" int geLinuxRender_ShouldClose(
    const geLinuxRender_Runtime *runtime) {
    return !runtime || runtime->close_requested;
}

extern "C" int geLinuxRender_GetFrameStats(
    const geLinuxRender_Runtime *runtime, geLinuxRender_FrameStats *stats) {
    if (!runtime || !stats)
        return 0;
    stats->VisibleFaces = runtime->diagnostics.visible_faces;
    stats->SubmodelFaces = runtime->diagnostics.submodel_faces;
    stats->ActorPrimitives = runtime->diagnostics.actor_submissions;
    stats->EffectPrimitives = static_cast<uint64_t>(
        runtime->beams.size() + runtime->billboards.size());
    stats->DynamicLights = runtime->diagnostics.dynamic_lights;
    stats->RegeneratedLightmaps = runtime->diagnostics.regenerated_lightmaps;
    return 1;
}

extern "C" int geLinuxRender_SubmitBeam(
    geLinuxRender_Runtime *runtime, const geLinuxRender_Beam *beam) {
    if (!runtime || !beam || !valid_beam(*beam))
        return 0;
    RuntimeBeam runtime_beam;
    runtime_beam.beam = *beam;
    runtime_beam.remaining = beam->LifetimeSeconds;
    runtime->beams.push_back(runtime_beam);
    return 1;
}

extern "C" size_t geLinuxRender_GetTransientEffectCount(
    const geLinuxRender_Runtime *runtime) {
    return runtime ? runtime->beams.size() + runtime->billboards.size() : 0U;
}

extern "C" int geLinuxRender_CreateDynamicLight(
    geLinuxRender_Runtime *runtime, const geLinuxRender_Vec3 *position,
    const geLinuxRender_Color3 *color, double radius, size_t *light_index) {
    if (!runtime || !runtime->world || !position || !color || !light_index ||
        !valid_light(*position, *color, radius))
        return 0;
    RuntimeLight light;
    light.native = geWorld_AddLight(runtime->world);
    if (!light.native)
        return 0;
    if (!set_runtime_light(runtime, light, *position, *color, radius)) {
        geWorld_RemoveLight(runtime->world, light.native);
        return 0;
    }
    runtime->dynamic_lights.push_back(light);
    *light_index = runtime->dynamic_lights.size() - 1U;
    return 1;
}

extern "C" int geLinuxRender_SetDynamicLight(
    geLinuxRender_Runtime *runtime, size_t light_index,
    const geLinuxRender_Vec3 *position,
    const geLinuxRender_Color3 *color, double radius) {
    if (!runtime || !position || !color ||
        light_index >= runtime->dynamic_lights.size())
        return 0;
    return set_runtime_light(runtime, runtime->dynamic_lights[light_index],
                             *position, *color, radius);
}

extern "C" int geLinuxRender_RemoveDynamicLight(
    geLinuxRender_Runtime *runtime, size_t light_index) {
    if (!runtime || !runtime->world ||
        light_index >= runtime->dynamic_lights.size() ||
        !runtime->dynamic_lights[light_index].native)
        return 0;
    geWorld_RemoveLight(runtime->world,
                        runtime->dynamic_lights[light_index].native);
    runtime->dynamic_lights[light_index].native = nullptr;
    if (!runtime->headless)
        mark_dynamic_lightmaps(runtime->world, &runtime->world_textures,
                               &runtime->lighting_state);
    return 1;
}

extern "C" int geLinuxRender_SubmitBillboard(
    geLinuxRender_Runtime *runtime,
    const geLinuxRender_Billboard *billboard) {
    if (!runtime || !billboard || !valid_billboard(*billboard))
        return 0;
    RuntimeBillboard runtime_billboard;
    runtime_billboard.billboard = *billboard;
    runtime_billboard.remaining = billboard->LifetimeSeconds;
    runtime->billboards.push_back(runtime_billboard);
    return 1;
}

extern "C" size_t geLinuxRender_GetMaterialCount(
    const geLinuxRender_Runtime *runtime) {
    if (!runtime || !runtime->world || !runtime->world->CurrentBSP)
        return 0U;
    const int32 count = runtime->world->CurrentBSP->BSPData.NumGFXTextures;
    return count > 0 ? static_cast<size_t>(count) : 0U;
}

extern "C" size_t geLinuxRender_GetMaterialName(
    const geLinuxRender_Runtime *runtime, size_t material_index_value,
    char *buffer, size_t buffer_size) {
    if (!runtime || !runtime->world || !runtime->world->CurrentBSP)
        return 0U;
    const GBSP_BSPData &bsp = runtime->world->CurrentBSP->BSPData;
    const size_t count = bsp.NumGFXTextures > 0
        ? static_cast<size_t>(bsp.NumGFXTextures) : 0U;
    if (material_index_value >= count)
        return 0U;
    const GFX_Texture &material = bsp.GFXTextures[material_index_value];
    const size_t length = strnlen(material.Name, sizeof(material.Name));
    return copy_api_span(material.Name, length, buffer, buffer_size);
}

extern "C" int geLinuxRender_GetMaterialInfo(
    const geLinuxRender_Runtime *runtime, size_t material_index_value,
    geLinuxRender_MaterialInfo *info) {
    if (!runtime || !runtime->world || !runtime->world->CurrentBSP || !info)
        return 0;
    const GBSP_BSPData &bsp = runtime->world->CurrentBSP->BSPData;
    const size_t count = bsp.NumGFXTextures > 0
        ? static_cast<size_t>(bsp.NumGFXTextures) : 0U;
    if (material_index_value >= count)
        return 0;
    const GFX_Texture &material = bsp.GFXTextures[material_index_value];
    if (material.Width <= 0 || material.Height <= 0)
        return 0;
    *info = {material.Width, material.Height};
    return 1;
}

extern "C" int geLinuxRender_UpdateMaterialRGBA(
    geLinuxRender_Runtime *runtime, const char *material_name,
    int width, int height, const uint8_t *pixels, size_t byte_count,
    size_t row_stride) {
    if (!runtime || !pixels || width <= 0 || height <= 0)
        return 0;
    const int index = material_index(runtime, material_name);
    if (index < 0)
        return 0;
    const GFX_Texture &material =
        runtime->world->CurrentBSP->BSPData.GFXTextures[index];
    if (width != material.Width || height != material.Height ||
        static_cast<size_t>(width) > SIZE_MAX / 4U)
        return 0;
    const size_t row_bytes = static_cast<size_t>(width) * 4U;
    if (row_stride < row_bytes ||
        static_cast<size_t>(height - 1) >
            (SIZE_MAX - row_bytes) / row_stride ||
        static_cast<size_t>(height - 1) * row_stride + row_bytes > byte_count)
        return 0;
    if (runtime->headless)
        return 1;
    if (!runtime->context || !runtime->window ||
        static_cast<size_t>(index) >= runtime->world_textures.materials.size() ||
        runtime->world_textures.materials[static_cast<size_t>(index)] == 0U ||
        SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0)
        return 0;

    const uint8_t *upload = pixels;
    std::vector<uint8_t> packed;
    if (row_stride != row_bytes) {
        packed.resize(row_bytes * static_cast<size_t>(height));
        for (int row = 0; row < height; ++row) {
            std::memcpy(packed.data() + static_cast<size_t>(row) * row_bytes,
                        pixels + static_cast<size_t>(row) * row_stride,
                        row_bytes);
        }
        upload = packed.data();
    }
    GLint previous_active_texture = 0;
    GLint previous_binding = 0;
    GLint previous_unpack_alignment = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_binding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    while (glGetError() != GL_NO_ERROR) {}
    glBindTexture(GL_TEXTURE_2D,
                  runtime->world_textures.materials[static_cast<size_t>(index)]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, upload);
    const GLenum upload_error = glGetError();
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_binding));
    glActiveTexture(static_cast<GLenum>(previous_active_texture));
    return upload_error == GL_NO_ERROR ? 1 : 0;
}

extern "C" int geLinuxRender_GetEntityOrigin(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index, geLinuxRender_Vec3 *origin) {
    geEntity *entity = entity_at(runtime, class_name, entity_index);
    if (!entity || !origin)
        return 0;
    const char *value = geEntity_GetStringForKey(entity, "origin");
    geVec3d parsed{};
    if (!value || std::sscanf(value, "%f %f %f", &parsed.X, &parsed.Y,
                              &parsed.Z) != 3)
        return 0;
    *origin = {parsed.X, parsed.Y, parsed.Z};
    return 1;
}

extern "C" int geLinuxRender_GetEntityModelBounds(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index, geLinuxRender_Aabb *bounds) {
    geEntity *entity = entity_at(runtime, class_name, entity_index);
    if (!entity || !bounds || !runtime->world || !runtime->world->CurrentBSP)
        return 0;
    int32 model_number = -1;
    if (geEntity_GetModelNumForKey(entity, "Model", &model_number) != GE_TRUE ||
        model_number <= 0 ||
        model_number >= runtime->world->CurrentBSP->BSPData.NumGFXModels)
        return 0;
    const geWorld_Model &model = runtime->world->CurrentBSP->Models[model_number];
    bounds->Minimum = {model.Mins.X, model.Mins.Y, model.Mins.Z};
    bounds->Maximum = {model.Maxs.X, model.Maxs.Y, model.Maxs.Z};
    return 1;
}

extern "C" int geLinuxRender_GetEntityModelIndex(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index, size_t *model_index) {
    geEntity *entity = entity_at(runtime, class_name, entity_index);
    int32 model_number = -1;
    if (!entity || !model_index || !runtime->world ||
        !runtime->world->CurrentBSP ||
        geEntity_GetModelNumForKey(entity, "Model", &model_number) != GE_TRUE ||
        model_number <= 0 ||
        model_number >= runtime->world->CurrentBSP->BSPData.NumGFXModels)
        return 0;
    *model_index = static_cast<size_t>(model_number);
    return 1;
}

extern "C" size_t geLinuxRender_GetWorldModelCount(
    const geLinuxRender_Runtime *runtime) {
    return runtime && runtime->world && runtime->world->CurrentBSP &&
                   runtime->world->CurrentBSP->BSPData.NumGFXModels > 0
        ? static_cast<size_t>(
              runtime->world->CurrentBSP->BSPData.NumGFXModels) : 0U;
}

extern "C" int geLinuxRender_FindWorldModel(
    const geLinuxRender_Runtime *runtime, const char *model_name,
    size_t *model_index) {
    const size_t model_count = geLinuxRender_GetWorldModelCount(runtime);
    if (!model_name || !*model_name || !model_index)
        return 0;
    for (size_t index = 1; index < model_count; ++index) {
        const char *name = runtime->world->CurrentBSP->Models[index].Name;
        if (name && strcasecmp(name, model_name) == 0) {
            *model_index = index;
            return 1;
        }
    }
    geEntity_EntitySet *all_entities = geWorld_GetEntitySet(
        runtime->world, nullptr);
    geEntity *named_entity = all_entities
        ? geEntity_EntitySetFindEntityByName(all_entities, model_name)
        : nullptr;
    int32 named_model = -1;
    if (named_entity &&
        geEntity_GetModelNumForKey(named_entity, "Model", &named_model) ==
            GE_TRUE &&
        named_model > 0 && static_cast<size_t>(named_model) < model_count) {
        *model_index = static_cast<size_t>(named_model);
        return 1;
    }
    return 0;
}

extern "C" int geLinuxRender_GetWorldModelBounds(
    const geLinuxRender_Runtime *runtime, size_t model_index,
    geLinuxRender_Aabb *bounds) {
    const size_t model_count = geLinuxRender_GetWorldModelCount(runtime);
    if (!bounds || model_index >= model_count)
        return 0;
    const geWorld_Model &model = runtime->world->CurrentBSP->Models[model_index];
    bounds->Minimum = {model.TMins.X, model.TMins.Y, model.TMins.Z};
    bounds->Maximum = {model.TMaxs.X, model.TMaxs.Y, model.TMaxs.Z};
    return 1;
}

extern "C" int geLinuxRender_SetWorldModelTransform(
    geLinuxRender_Runtime *runtime, size_t model_index,
    const geLinuxRender_Vec3 *translation,
    const geLinuxRender_Vec3 *euler_radians) {
    const size_t model_count = geLinuxRender_GetWorldModelCount(runtime);
    if (!translation || !euler_radians || model_index == 0 ||
        model_index >= model_count || !valid_vector(*translation) ||
        !valid_vector(*euler_radians))
        return 0;
    const geVec3d angles{static_cast<geFloat>(euler_radians->X),
                         static_cast<geFloat>(euler_radians->Y),
                         static_cast<geFloat>(euler_radians->Z)};
    geXForm3d transform;
    geXForm3d_SetEulerAngles(&transform, &angles);
    transform.Translation = {static_cast<geFloat>(translation->X),
                             static_cast<geFloat>(translation->Y),
                             static_cast<geFloat>(translation->Z)};
    return geWorld_SetModelXForm(
               runtime->world,
               &runtime->world->CurrentBSP->Models[model_index],
               &transform) == GE_TRUE;
}

extern "C" int geLinuxRender_GetWorldModelMotionExtents(
    const geLinuxRender_Runtime *runtime, size_t model_index,
    double *start_seconds, double *end_seconds) {
    const size_t model_count = geLinuxRender_GetWorldModelCount(runtime);
    if (!start_seconds || !end_seconds || model_index == 0 ||
        model_index >= model_count)
        return 0;
    geMotion *motion = geWorld_ModelGetMotion(
        &runtime->world->CurrentBSP->Models[model_index]);
    geFloat start = 0.0f;
    geFloat end = 0.0f;
    if (!motion || !geMotion_GetTimeExtents(motion, &start, &end) ||
        end <= start || !geMotion_GetPath(motion, 0))
        return 0;
    *start_seconds = start;
    *end_seconds = end;
    return 1;
}

extern "C" int geLinuxRender_SetWorldModelMotionTime(
    geLinuxRender_Runtime *runtime, size_t model_index,
    double time_seconds) {
    const size_t model_count = geLinuxRender_GetWorldModelCount(runtime);
    if (model_index == 0 || model_index >= model_count ||
        !std::isfinite(time_seconds))
        return 0;
    geWorld_Model *model =
        &runtime->world->CurrentBSP->Models[model_index];
    geMotion *motion = geWorld_ModelGetMotion(model);
    gePath *path = motion ? geMotion_GetPath(motion, 0) : nullptr;
    geXForm3d transform;
    if (!path)
        return 0;
    gePath_Sample(path, static_cast<geFloat>(time_seconds), &transform);
    return geWorld_SetModelXForm(runtime->world, model, &transform) == GE_TRUE;
}

extern "C" int geLinuxRender_SetWorldModelVisible(
    geLinuxRender_Runtime *runtime, size_t model_index, int visible) {
    if (!runtime || model_index == 0 ||
        model_index >= runtime->model_visibility.size())
        return 0;
    runtime->model_visibility[model_index] = visible ? 1U : 0U;
    return 1;
}

extern "C" size_t geLinuxRender_GetEntityClassCount(
    const geLinuxRender_Runtime *runtime) {
    return runtime && runtime->world && runtime->world->NumEntClassSets > 1
        ? static_cast<size_t>(runtime->world->NumEntClassSets - 1) : 0U;
}

extern "C" size_t geLinuxRender_GetEntityClassName(
    const geLinuxRender_Runtime *runtime, size_t class_index,
    char *buffer, size_t buffer_size) {
    const size_t class_count = geLinuxRender_GetEntityClassCount(runtime);
    if (class_index >= class_count)
        return 0;
    return copy_api_string(runtime->world->EntClassSets[class_index + 1].ClassName,
                           buffer, buffer_size);
}

extern "C" size_t geLinuxRender_GetEntityCount(
    const geLinuxRender_Runtime *runtime, const char *class_name) {
    if (!runtime || !runtime->world || !class_name)
        return 0;
    geEntity_EntitySet *set = geWorld_GetEntitySet(runtime->world, class_name);
    if (!set)
        return 0;
    size_t count = 0;
    for (geEntity *entity = geEntity_EntitySetGetNextEntity(set, nullptr);
         entity; entity = geEntity_EntitySetGetNextEntity(set, entity))
        ++count;
    return count;
}

extern "C" size_t geLinuxRender_GetEntityKeyCount(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index) {
    const geEntity *entity = entity_at(runtime, class_name, entity_index);
    size_t count = 0;
    for (const geEntity_Epair *pair = entity ? entity->Epairs : nullptr;
         pair; pair = pair->Next)
        ++count;
    return count;
}

extern "C" size_t geLinuxRender_GetEntityKeyName(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index, size_t key_index, char *buffer,
    size_t buffer_size) {
    const geEntity *entity = entity_at(runtime, class_name, entity_index);
    const geEntity_Epair *pair = entity_epair_at(entity, key_index);
    return copy_api_string(pair ? pair->Key : nullptr, buffer, buffer_size);
}

extern "C" size_t geLinuxRender_GetEntityValue(
    const geLinuxRender_Runtime *runtime, const char *class_name,
    size_t entity_index, const char *key, char *buffer, size_t buffer_size) {
    const geEntity *entity = entity_at(runtime, class_name, entity_index);
    return copy_api_string(entity && key ? geEntity_GetStringForKey(entity, key)
                                         : nullptr,
                           buffer, buffer_size);
}

extern "C" int geLinuxRender_TraceWorld(
    const geLinuxRender_Runtime *runtime, const geLinuxRender_Vec3 *start,
    const geLinuxRender_Vec3 *end, geLinuxRender_TraceResult *result) {
    if (!runtime || !runtime->world || !start || !end || !result)
        return 0;
    if (!valid_vector(*start) || !valid_vector(*end))
        return 0;

    *result = geLinuxRender_TraceResult{};
    const geVec3d front{static_cast<geFloat>(start->X),
                        static_cast<geFloat>(start->Y),
                        static_cast<geFloat>(start->Z)};
    const geVec3d back{static_cast<geFloat>(end->X),
                       static_cast<geFloat>(end->Y),
                       static_cast<geFloat>(end->Z)};
    GE_Collision collision{};
    if (geWorld_Collision(runtime->world, nullptr, nullptr, &front, &back,
                          GE_CONTENTS_SOLID_CLIP, GE_COLLIDE_ALL,
                          0xffffffffU, nullptr, nullptr,
                          &collision) == GE_FALSE) {
        return 1;
    }

    export_collision(collision, result);
    return 1;
}

extern "C" int geLinuxRender_SweepWorld(
    const geLinuxRender_Runtime *runtime,
    const geLinuxRender_Aabb *local_bounds,
    const geLinuxRender_Vec3 *start, const geLinuxRender_Vec3 *end,
    geLinuxRender_TraceResult *result) {
    if (!runtime || !runtime->world || !local_bounds || !start || !end ||
        !result || !valid_vector(local_bounds->Minimum) ||
        !valid_vector(local_bounds->Maximum) || !valid_vector(*start) ||
        !valid_vector(*end) ||
        local_bounds->Minimum.X > local_bounds->Maximum.X ||
        local_bounds->Minimum.Y > local_bounds->Maximum.Y ||
        local_bounds->Minimum.Z > local_bounds->Maximum.Z)
        return 0;

    *result = geLinuxRender_TraceResult{};
    const geVec3d minimum{static_cast<geFloat>(local_bounds->Minimum.X),
                          static_cast<geFloat>(local_bounds->Minimum.Y),
                          static_cast<geFloat>(local_bounds->Minimum.Z)};
    const geVec3d maximum{static_cast<geFloat>(local_bounds->Maximum.X),
                          static_cast<geFloat>(local_bounds->Maximum.Y),
                          static_cast<geFloat>(local_bounds->Maximum.Z)};
    const geVec3d front{static_cast<geFloat>(start->X),
                        static_cast<geFloat>(start->Y),
                        static_cast<geFloat>(start->Z)};
    const geVec3d back{static_cast<geFloat>(end->X),
                       static_cast<geFloat>(end->Y),
                       static_cast<geFloat>(end->Z)};
    GE_Collision collision{};
    if (geWorld_Collision(runtime->world, &minimum, &maximum, &front, &back,
                          GE_CONTENTS_SOLID_CLIP, GE_COLLIDE_MODELS,
                          0xffffffffU, nullptr, nullptr,
                          &collision) == GE_FALSE) {
        return 1;
    }
    export_collision(collision, result);
    return 1;
}

extern "C" size_t geLinuxRender_GetActorCount(
    const geLinuxRender_Runtime *runtime) {
    return runtime ? runtime->actors.size() : 0U;
}

extern "C" int geLinuxRender_CreateActor(
    geLinuxRender_Runtime *runtime, const char *actor_path,
    const geLinuxRender_Vec3 *position, double yaw_radians,
    size_t *actor_index) {
    constexpr size_t maximum_actor_slots = 4096;
    if (!runtime || !actor_path || !*actor_path || !position || !actor_index ||
        !valid_vector(*position) || !std::isfinite(yaw_radians) ||
        runtime->actors.size() >= maximum_actor_slots)
        return 0;
    if (!runtime->headless &&
        SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("could not activate OpenGL context for actor creation: ") +
                      SDL_GetError());
        return 0;
    }
    std::unique_ptr<RuntimeActor> actor(
        new (std::nothrow) RuntimeActor());
    if (!actor) {
        set_api_error("out of memory creating actor slot");
        return 0;
    }
    if (!load_native_actor(actor_path, &actor->native, !runtime->headless)) {
        set_api_error("could not load actor asset");
        return 0;
    }
    geXForm3d transform;
    geXForm3d_SetYRotation(&transform, static_cast<geFloat>(yaw_radians));
    transform.Translation = {static_cast<geFloat>(position->X),
                             static_cast<geFloat>(position->Y),
                             static_cast<geFloat>(position->Z)};
    set_actor_root_transform(&actor->native, transform);
    *actor_index = runtime->actors.size();
    runtime->actors.push_back(std::move(actor));
    return 1;
}

extern "C" int geLinuxRender_RemoveActor(geLinuxRender_Runtime *runtime,
                                          size_t actor_index) {
    RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor)
        return 0;
    if (!runtime->headless &&
        SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("could not activate OpenGL context for actor removal: ") +
                      SDL_GetError());
        return 0;
    }
    destroy_native_actor(&actor->native);
    runtime->actors[actor_index].reset();
    return 1;
}

extern "C" int geLinuxRender_ClearActors(geLinuxRender_Runtime *runtime) {
    if (!runtime)
        return 0;
    if (!runtime->headless &&
        SDL_GL_MakeCurrent(runtime->window, runtime->context) != 0) {
        set_api_error(std::string("could not activate OpenGL context for actor cleanup: ") +
                      SDL_GetError());
        return 0;
    }
    for (auto &actor : runtime->actors) {
        if (actor)
            destroy_native_actor(&actor->native);
    }
    runtime->actors.clear();
    return 1;
}

extern "C" int geLinuxRender_SetActorTransform(
    geLinuxRender_Runtime *runtime, size_t actor_index,
    const geLinuxRender_Vec3 *position, double yaw_radians) {
    RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !actor->native.actor ||
        !position || !valid_vector(*position) || !std::isfinite(yaw_radians))
        return 0;
    geXForm3d transform;
    geXForm3d_SetYRotation(&transform, static_cast<geFloat>(yaw_radians));
    transform.Translation = {static_cast<geFloat>(position->X),
                             static_cast<geFloat>(position->Y),
                             static_cast<geFloat>(position->Z)};
    set_actor_root_transform(&actor->native, transform);
    return 1;
}

extern "C" int geLinuxRender_GetActorBounds(
    const geLinuxRender_Runtime *runtime, size_t actor_index,
    geLinuxRender_Aabb *bounds) {
    const RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !actor->native.actor || !bounds)
        return 0;
    geExtBox box{};
    if (geActor_GetDynamicExtBox(actor->native.actor, &box) == GE_FALSE)
        return 0;
    bounds->Minimum = {box.Min.X, box.Min.Y, box.Min.Z};
    bounds->Maximum = {box.Max.X, box.Max.Y, box.Max.Z};
    return 1;
}

extern "C" int geLinuxRender_SetActorVisible(
    geLinuxRender_Runtime *runtime, size_t actor_index, int visible) {
    RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !actor->native.actor)
        return 0;
    actor->visible = visible != 0;
    return 1;
}

extern "C" size_t geLinuxRender_GetActorMotionCount(
    const geLinuxRender_Runtime *runtime, size_t actor_index) {
    const RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    return actor && actor->native.motion_count > 0
        ? static_cast<size_t>(actor->native.motion_count) : 0U;
}

extern "C" size_t geLinuxRender_GetActorMotionName(
    const geLinuxRender_Runtime *runtime, size_t actor_index,
    size_t motion_index, char *buffer, size_t buffer_size) {
    const RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !actor->native.definition ||
        motion_index >= static_cast<size_t>(actor->native.motion_count))
        return 0;
    return copy_api_string(
        geActor_GetMotionName(actor->native.definition,
                              static_cast<int>(motion_index)),
        buffer, buffer_size);
}

extern "C" int geLinuxRender_SetActorMotion(
    geLinuxRender_Runtime *runtime, size_t actor_index,
    const char *motion_name, int restart) {
    RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !actor->native.definition || !actor->native.actor ||
        !motion_name || !*motion_name)
        return 0;
    geMotion *motion = geActor_GetMotionByName(actor->native.definition,
                                               motion_name);
    if (!motion) {
        const std::string requested = normalized_motion_name(motion_name);
        for (int index = 0; index < actor->native.motion_count; ++index) {
            const char *candidate = geActor_GetMotionName(
                actor->native.definition, index);
            if (candidate &&
                normalized_motion_name(candidate) == requested) {
                motion = geActor_GetMotionByIndex(actor->native.definition,
                                                  index);
                break;
            }
        }
    }
    geFloat start = 0.0f;
    geFloat end = 0.0f;
    if (!motion || geMotion_GetTimeExtents(motion, &start, &end) != GE_TRUE ||
        end <= start)
        return 0;
    if (actor->native.motion == motion && !restart)
        return 1;
    actor->native.motion = motion;
    actor->native.motion_start = start;
    actor->native.motion_end = end;
    actor->native.motion_time = start;
    for (int index = 0; index < actor->native.motion_count; ++index) {
        if (geActor_GetMotionByIndex(actor->native.definition, index) == motion) {
            actor->native.motion_index = index;
            break;
        }
    }
    geActor_SetPose(actor->native.actor, motion, start,
                    &actor->native.root_transform);
    return 1;
}

extern "C" int geLinuxRender_SetActorScale(
    geLinuxRender_Runtime *runtime, size_t actor_index,
    const geLinuxRender_Vec3 *scale) {
    RuntimeActor *actor = runtime_actor_at(runtime, actor_index);
    if (!actor || !scale || !valid_vector(*scale) ||
        scale->X <= 0.0 || scale->Y <= 0.0 || scale->Z <= 0.0)
        return 0;
    actor->native.scale = {static_cast<geFloat>(scale->X),
                           static_cast<geFloat>(scale->Y),
                           static_cast<geFloat>(scale->Z)};
    return 1;
}

extern "C" int geLinuxRender_SetOverlayCallback(
    geLinuxRender_Runtime *runtime, geLinuxRender_OverlayCallback callback,
    void *context) {
    if (!runtime)
        return 0;
    runtime->overlay_callback = callback;
    runtime->overlay_context = callback ? context : nullptr;
    return 1;
}

extern "C" const char *geLinuxRender_GetLastError(void) {
    return render_api_error.c_str();
}
