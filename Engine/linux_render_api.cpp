/*
 * The contents of this file are subject to the Genesis3D Public License
 * Version 1.01. See ../g3dlicense.txt. Contributor: Kadaken, 2026.
 */

#define GENESIS3D_RENDER_LIBRARY_IMPLEMENTATION 1
#include "../linux_main.cpp"

#include "LinuxRender.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <new>

struct RuntimeActor {
    NativeActor native;
    bool visible = true;
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
    NativeLightingState lighting_state;
    RenderDiagnostics diagnostics;
    PlayerCamera camera{{0.0, 0.0, 0.0}, 0.0, 0.0, 400.0};
    PlayerPhysics player_physics;
    HeldMovementInput movement_input;
    bool fire_button = false;
    geLinuxRender_OverlayCallback overlay_callback = nullptr;
    void *overlay_context = nullptr;
};

namespace {

thread_local std::string render_api_error;

void set_api_error(const std::string &message) {
    render_api_error = message;
    std::fprintf(stderr, "Genesis3D render API: %s\n", message.c_str());
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

    destroy_face_cache(&runtime->face_cache);
    destroy_world_textures(&runtime->world_textures);
    geWorld_Free(runtime->world);
    runtime->world = replacement_world;
    runtime->world_textures = std::move(replacement_textures);
    runtime->face_cache = std::move(replacement_cache);
    runtime->lighting_state = NativeLightingState{};
    runtime->diagnostics = RenderDiagnostics{};
    runtime->player_physics = PlayerPhysics{};
    runtime->movement_input = HeldMovementInput{};
    runtime->fire_button = false;
    runtime->camera = initial_camera(runtime->world);
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

extern "C" int geLinuxRender_AdvanceSimulation(
    geLinuxRender_Runtime *runtime, double fixed_delta_seconds) {
    if (!runtime || !std::isfinite(fixed_delta_seconds) ||
        fixed_delta_seconds < 0.0)
        return 0;
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
                               runtime->framebuffer_height)) {
        set_api_error("world has no renderable BSP geometry");
        return 0;
    }
    for (size_t index = 1; index < runtime->actors.size(); ++index) {
        RuntimeActor *actor = runtime_actor_at(runtime, index);
        if (actor && actor->visible)
            render_native_actor(&actor->native, &runtime->diagnostics);
    }
    if (runtime->overlay_callback)
        runtime->overlay_callback(runtime->overlay_context,
                                  runtime->framebuffer_width,
                                  runtime->framebuffer_height);
    SDL_GL_SwapWindow(runtime->window);
    return 1;
}

extern "C" int geLinuxRender_ShouldClose(
    const geLinuxRender_Runtime *runtime) {
    return !runtime || runtime->close_requested;
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
