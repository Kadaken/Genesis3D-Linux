/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Asset-free actor keyframe/pose discriminator.
 */
#include "Genesis.h"
#include "body.h"
#include "bodyinst.h"
#include "motion.h"
#include "path.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "actor pose fixture: stage=%s line=%d: %s\n", \
        stage, __LINE__, #expr); goto cleanup; \
} } while (0)

static const geFloat tolerance = 1.0e-4f;

static int near_scalar(geFloat actual, geFloat expected)
{
    return fabsf(actual - expected) <= tolerance;
}

static int near_point(const geVec3d *actual, geFloat x, geFloat y, geFloat z)
{
    return near_scalar(actual->X, x) && near_scalar(actual->Y, y) &&
        near_scalar(actual->Z, z);
}

static int check_vertex(const geActor *actor, const char *bone,
    const geVec3d *local, geFloat x, geFloat y, geFloat z)
{
    geXForm3d transform;
    geVec3d actual;
    if (!geActor_GetBoneTransform(actor, bone, &transform))
        return 0;
    geXForm3d_Transform(&transform, local, &actual);
    fprintf(stderr, "actor pose: bone=%s vertex=%.6f/%.6f/%.6f expected=%.6f/%.6f/%.6f\n",
        bone, actual.X, actual.Y, actual.Z, x, y, z);
    return near_point(&actual, x, y, z);
}

static int check_geometry(const geActor *actor, const geActor_Def *definition,
    const geVec3d expected[3])
{
    geBodyInst *instance = geBodyInst_Create(geActor_GetBody(definition));
    const geBodyInst_Geometry *geometry;
    geVec3d scale;
    int i, result = 0;
    if (!instance)
        return 0;
    geVec3d_Set(&scale, 1.0f, 1.0f, 1.0f);
    geometry = geBodyInst_GetGeometry(instance, &scale,
        geActor_GetPoseTransforms(actor), 0, NULL);
    if (!geometry || geometry->SkinVertexCount != 3)
        goto done;
    for (i = 0; i < 3; ++i) {
        fprintf(stderr, "actor skin: vertex=%d actual=%.6f/%.6f/%.6f expected=%.6f/%.6f/%.6f\n",
            i, geometry->SkinVertexArray[i].SVPoint.X,
            geometry->SkinVertexArray[i].SVPoint.Y,
            geometry->SkinVertexArray[i].SVPoint.Z,
            expected[i].X, expected[i].Y, expected[i].Z);
        if (!near_point(&geometry->SkinVertexArray[i].SVPoint,
                expected[i].X, expected[i].Y, expected[i].Z))
            goto done;
    }
    result = 1;
done:
    geBodyInst_Destroy(&instance);
    return result;
}

static gePath *make_path(geFloat radians)
{
    gePath *path = gePath_Create(GE_PATH_INTERPOLATE_LINEAR,
        GE_PATH_INTERPOLATE_SLERP, GE_FALSE);
    geXForm3d key;
    if (!path)
        return NULL;
    geXForm3d_SetZRotation(&key, radians);
    if (!gePath_InsertKeyframe(path, GE_PATH_ALL_CHANNELS, 0.0f, &key)) {
        gePath_Destroy(&path);
        return NULL;
    }
    return path;
}

int main(void)
{
    static const char *names[] = {"root", "child", "tip"};
    static const geFloat rotations[] = {0.0f, 1.5707963267948966f,
        -1.5707963267948966f};
    char directory[] = "/tmp/genesis-actor-pose-XXXXXX";
    const char *stage = "setup";
    geActor_Def *definition = NULL, *loaded_definition = NULL;
    geActor *actor = NULL;
    geBody *body = NULL;
    geMotion *motion = NULL, *loaded_motion;
    gePath *path = NULL;
    geVFile *file = NULL;
    geXForm3d attachment, sampled;
    geVec3d local, normal, vertices[3], rest_expected[3], animated_expected[3];
    int bone, path_index, motion_index, material_index;
    int result = 1;

    CHECK(mkdtemp(directory));
    CHECK(chdir(directory) == 0);

    stage = "construct body";
    definition = geActor_DefCreate();
    body = geBody_Create();
    CHECK(definition && body);
    geXForm3d_SetTranslation(&attachment, 10.0f, 0.0f, 0.0f);
    CHECK(geBody_AddBone(body, GE_BODY_NO_PARENT_BONE, names[0], &attachment, &bone));
    CHECK(bone == 0);
    geXForm3d_SetTranslation(&attachment, 0.0f, 5.0f, 0.0f);
    CHECK(geBody_AddBone(body, 0, names[1], &attachment, &bone));
    CHECK(bone == 1);
    geXForm3d_SetTranslation(&attachment, 0.0f, 4.0f, 0.0f);
    CHECK(geBody_AddBone(body, 1, names[2], &attachment, &bone));
    CHECK(bone == 2);
    CHECK(geBody_AddMaterial(body, "neutral", NULL, 255.0f, 255.0f, 255.0f,
        &material_index));
    CHECK(material_index == 0);
    geVec3d_Set(&normal, 0.0f, 0.0f, 1.0f);
    geVec3d_Set(&vertices[0], 1.0f, 0.0f, 0.0f);
    geVec3d_Set(&vertices[1], 1.0f, 0.0f, 0.0f);
    geVec3d_Set(&vertices[2], 1.0f, 0.0f, 0.0f);
    CHECK(geBody_AddFace(body,
        &vertices[0], &normal, 0.0f, 0.0f, 0,
        &vertices[1], &normal, 1.0f, 0.0f, 1,
        &vertices[2], &normal, 0.0f, 1.0f, 2, material_index));
    CHECK(geActor_SetBody(definition, body));
    body = NULL;

    stage = "construct keyframes";
    motion = geMotion_Create(GE_TRUE);
    CHECK(motion);
    CHECK(geMotion_SetName(motion, "known rotation"));
    for (bone = 0; bone < 3; ++bone) {
        path = make_path(rotations[bone]);
        CHECK(path);
        CHECK(geMotion_AddPath(motion, path, names[bone], &path_index));
        CHECK(path_index == bone);
        gePath_Destroy(&path);
        /* The legacy shared-owner release decrements its reference without
         * clearing the caller's alias.  Drop that non-owning alias here. */
        path = NULL;
    }
    CHECK(geActor_AddMotion(definition, motion, &motion_index));
    motion = NULL;

    stage = "write actor";
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, "pose.act", NULL,
        GE_VFILE_OPEN_CREATE);
    CHECK(file);
    CHECK(geActor_DefWriteToFile(definition, file));
    CHECK(geVFile_Close(file));
    file = NULL;
    CHECK(geActor_DefDestroy(&definition));

    stage = "reload actor";
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, "pose.act", NULL,
        GE_VFILE_OPEN_READONLY);
    CHECK(file);
    loaded_definition = geActor_DefCreateFromFile(file);
    CHECK(loaded_definition);
    CHECK(geVFile_Close(file));
    file = NULL;
    CHECK(geActor_GetMotionCount(loaded_definition) == 1);
    loaded_motion = geActor_GetMotionByIndex(loaded_definition, 0);
    CHECK(loaded_motion);

    stage = "decode keyframes";
    for (bone = 0; bone < 3; ++bone) {
        CHECK(geMotion_SampleNamed(loaded_motion, names[bone], 0.0f, &sampled));
        CHECK(near_scalar(sampled.AX, cosf(rotations[bone])));
        CHECK(near_scalar(sampled.AY, -sinf(rotations[bone])));
        CHECK(near_scalar(sampled.BX, sinf(rotations[bone])));
        CHECK(near_scalar(sampled.BY, cosf(rotations[bone])));
        CHECK(near_point(&sampled.Translation, 0.0f, 0.0f, 0.0f));
    }

    stage = "compose rest pose";
    actor = geActor_Create(loaded_definition);
    CHECK(actor);
    geActor_ClearPose(actor, NULL);
    geVec3d_Set(&local, 1.0f, 0.0f, 0.0f);
    CHECK(check_vertex(actor, names[0], &local, 11.0f, 0.0f, 0.0f));
    CHECK(check_vertex(actor, names[1], &local, 11.0f, 5.0f, 0.0f));
    CHECK(check_vertex(actor, names[2], &local, 11.0f, 9.0f, 0.0f));
    geVec3d_Set(&rest_expected[0], 11.0f, 0.0f, 0.0f);
    geVec3d_Set(&rest_expected[1], 11.0f, 5.0f, 0.0f);
    geVec3d_Set(&rest_expected[2], 11.0f, 9.0f, 0.0f);
    CHECK(check_geometry(actor, loaded_definition, rest_expected));

    stage = "compose animated pose";
    geActor_SetPose(actor, loaded_motion, 0.0f, NULL);
    CHECK(check_vertex(actor, names[0], &local, 11.0f, 0.0f, 0.0f));
    CHECK(check_vertex(actor, names[1], &local, 10.0f, 6.0f, 0.0f));
    CHECK(check_vertex(actor, names[2], &local, 7.0f, 5.0f, 0.0f));
    geVec3d_Set(&animated_expected[0], 11.0f, 0.0f, 0.0f);
    geVec3d_Set(&animated_expected[1], 10.0f, 6.0f, 0.0f);
    geVec3d_Set(&animated_expected[2], 7.0f, 5.0f, 0.0f);
    CHECK(check_geometry(actor, loaded_definition, animated_expected));

    fprintf(stderr, "actor pose fixture: PASS two-bone and three-bone chains\n");
    result = 0;

cleanup:
    if (file) geVFile_Close(file);
    if (path) gePath_Destroy(&path);
    if (motion) geMotion_Destroy(&motion);
    if (actor) geActor_Destroy(&actor);
    if (loaded_definition) geActor_DefDestroy(&loaded_definition);
    if (definition) geActor_DefDestroy(&definition);
    if (body) geBody_Destroy(&body);
    geVFile_CloseAPI();
    if (access("pose.act", F_OK) == 0) unlink("pose.act");
    if (chdir("/") == 0) rmdir(directory);
    return result;
}
