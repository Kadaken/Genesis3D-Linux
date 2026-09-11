/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Asset-free BODY 00F1/00F2 skinning fixture.
 */
#include "body.h"
#include "bodyinst.h"
#include "xfarray.h"
#include "vfile.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "smooth skinning fixture: %s:%d: %s\n", stage, __LINE__, #value); \
    goto cleanup; \
} } while (0)

static int near_value(geFloat actual, geFloat expected)
{
    return fabsf(actual - expected) <= 1.0e-4f;
}

static geBody *make_body(int weighted)
{
    geBody *body = geBody_Create();
    geXForm3d identity;
    geVec3d vertices[3], normal;
    int bone, material;
    if (!body) return NULL;
    geXForm3d_SetIdentity(&identity);
    if (!geBody_AddBone(body, GE_BODY_NO_PARENT_BONE, "root", &identity, &bone) || bone != 0 ||
        !geBody_AddBone(body, 0, "joint", &identity, &bone) || bone != 1 ||
        !geBody_AddMaterial(body, "neutral", NULL, 255.0f, 255.0f, 255.0f, &material))
        goto failed;
    geVec3d_Set(&vertices[0], 1.0f, 0.0f, 0.0f);
    geVec3d_Set(&vertices[1], 2.0f, 0.0f, 0.0f);
    geVec3d_Set(&vertices[2], 1.0f, 1.0f, 0.0f);
    geVec3d_Set(&normal, 1.0f, 0.0f, 0.0f);
    if (!weighted) {
        if (!geBody_AddFace(body, &vertices[0], &normal, 0, 0, 1,
                &vertices[1], &normal, 1, 0, 1,
                &vertices[2], &normal, 0, 1, 1, material)) goto failed;
    } else {
        geBody_WeightedCorner corners[3];
        int i;
        memset(corners, 0, sizeof(corners));
        if (!geBody_SetBoneInverseBind(body, 0, &identity) ||
            !geBody_SetBoneInverseBind(body, 1, &identity)) goto failed;
        for (i = 0; i < 3; ++i) {
            corners[i].Vertex = vertices[i];
            corners[i].Normal = normal;
            corners[i].U = (i == 1) ? 1.0f : 0.0f;
            corners[i].V = (i == 2) ? 1.0f : 0.0f;
            corners[i].InfluenceCount = 2;
            corners[i].Influences[0].BoneIndex = 0;
            corners[i].Influences[0].Weight = 32768;
            corners[i].Influences[1].BoneIndex = 1;
            corners[i].Influences[1].Weight = 32767;
        }
		{
			geBody_WeightedCorner invalid[3];
			memcpy(invalid, corners, sizeof(invalid));
			invalid[0].InfluenceCount = 0;
			if (geBody_AddWeightedFace(body, invalid, material)) goto failed;
			memcpy(invalid, corners, sizeof(invalid));
			invalid[0].Influences[1].Weight = 32766;
			if (geBody_AddWeightedFace(body, invalid, material)) goto failed;
			memcpy(invalid, corners, sizeof(invalid));
			invalid[0].Influences[1].BoneIndex = 0;
			if (geBody_AddWeightedFace(body, invalid, material)) goto failed;
			memcpy(invalid, corners, sizeof(invalid));
			invalid[0].Influences[1].BoneIndex = 2;
			if (geBody_AddWeightedFace(body, invalid, material)) goto failed;
		}
        if (!geBody_AddWeightedFace(body, corners, material)) goto failed;
    }
    return body;
failed:
    geBody_Destroy(&body);
    return NULL;
}

static int write_body(const char *name, const geBody *body)
{
    geVFile *file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, name, NULL,
        GE_VFILE_OPEN_CREATE);
    int result;
    if (!file) return 0;
    result = geBody_WriteToFile(body, file);
    return geVFile_Close(file) && result;
}

static geBody *read_body(const char *name)
{
    geVFile *file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, name, NULL,
        GE_VFILE_OPEN_READONLY);
    geBody *body;
    if (!file) return NULL;
    body = geBody_CreateFromFile(file);
    geVFile_Close(file);
    return body;
}

static int files_equal(const char *left, const char *right)
{
    FILE *a = fopen(left, "rb"), *b = fopen(right, "rb");
    int ca, cb, equal = 0;
    if (!a || !b) goto done;
    do {
        ca = fgetc(a); cb = fgetc(b);
        if (ca != cb) goto done;
    } while (ca != EOF);
    equal = 1;
done:
    if (a) fclose(a);
    if (b) fclose(b);
    return equal;
}

static int check_pose(const geBody *body, geXFArray *transforms,
    geFloat x, geFloat y, geFloat nx, geFloat ny)
{
    geBodyInst *instance = geBodyInst_Create(body);
    const geBodyInst_Geometry *geometry;
    geVec3d scale;
    int result = 0;
    if (!instance) return 0;
    geVec3d_Set(&scale, 1, 1, 1);
    geometry = geBodyInst_GetGeometry(instance, &scale, transforms, 0, NULL);
    if (geometry && geometry->SkinVertexCount == 3 && geometry->NormalCount >= 1 &&
        near_value(geometry->SkinVertexArray[0].SVPoint.X, x) &&
        near_value(geometry->SkinVertexArray[0].SVPoint.Y, y) &&
        near_value(geometry->NormalArray[0].X, nx) &&
        near_value(geometry->NormalArray[0].Y, ny)) result = 1;
    if (geometry)
        fprintf(stderr, "smooth skinning: point=%.6f/%.6f normal=%.6f/%.6f\n",
            geometry->SkinVertexArray[0].SVPoint.X,
            geometry->SkinVertexArray[0].SVPoint.Y,
            geometry->NormalArray[0].X, geometry->NormalArray[0].Y);
    geBodyInst_Destroy(&instance);
    return result;
}

static double benchmark_pose(const geBody *body, geXFArray *transforms, int iterations)
{
    geBodyInst *instance = geBodyInst_Create(body);
    struct timespec begin, end;
    geVec3d scale;
    int i;
    double elapsed;
    if (!instance) return -1.0;
    geVec3d_Set(&scale, 1, 1, 1);
    clock_gettime(CLOCK_MONOTONIC, &begin);
    for (i = 0; i < iterations; ++i)
        if (!geBodyInst_GetGeometry(instance, &scale, transforms, 0, NULL)) {
            geBodyInst_Destroy(&instance);
            return -1.0;
        }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (double)(end.tv_sec - begin.tv_sec) * 1.0e9 +
        (double)(end.tv_nsec - begin.tv_nsec);
    geBodyInst_Destroy(&instance);
    return elapsed / (double)iterations;
}

int main(void)
{
    char directory[] = "/tmp/genesis-smooth-skinning-XXXXXX";
    const char *stage = "setup";
    geBody *rigid = NULL, *rigid_reloaded = NULL, *weighted = NULL, *weighted_reloaded = NULL;
    geXFArray *transforms = NULL;
    geXForm3d *elements;
    int count, result = 1;
    double rigid_ns, weighted_ns;
    const geFloat half_normal = 0.7071067811865475f;

    CHECK(mkdtemp(directory));
    CHECK(chdir(directory) == 0);
    stage = "construct";
    rigid = make_body(0);
    weighted = make_body(1);
    transforms = geXFArray_Create(2);
    CHECK(rigid && weighted && transforms);
    elements = geXFArray_GetElements(transforms, &count);
    CHECK(elements && count == 2);
    geXForm3d_SetIdentity(&elements[0]);
    geXForm3d_SetZRotation(&elements[1], 1.5707963267948966f);

    stage = "right-angle pose";
    CHECK(check_pose(rigid, transforms, 0.0f, 1.0f, 0.0f, 1.0f));
    CHECK(check_pose(weighted, transforms, 0.5000076f, 0.4999924f,
        half_normal, half_normal));

    stage = "write and reload";
    CHECK(write_body("rigid-a.body", rigid));
    rigid_reloaded = read_body("rigid-a.body");
    CHECK(rigid_reloaded);
    CHECK(write_body("rigid-b.body", rigid_reloaded));
    CHECK(files_equal("rigid-a.body", "rigid-b.body"));
    CHECK(write_body("weighted.body", weighted));
    weighted_reloaded = read_body("weighted.body");
    CHECK(weighted_reloaded);
    CHECK(check_pose(weighted_reloaded, transforms, 0.5000076f, 0.4999924f,
        half_normal, half_normal));

    stage = "pose benchmark";
    rigid_ns = benchmark_pose(rigid, transforms, 200000);
    weighted_ns = benchmark_pose(weighted, transforms, 200000);
    CHECK(rigid_ns > 0.0 && weighted_ns > 0.0);
    fprintf(stderr,
        "smooth skinning benchmark: rigid_ns=%.2f weighted_ns=%.2f ratio=%.3f vertices=3\n",
        rigid_ns, weighted_ns, weighted_ns / rigid_ns);

    fprintf(stderr, "smooth skinning fixture: PASS F1 byte identity and F2 blend\n");
    result = 0;
cleanup:
    if (transforms) geXFArray_Destroy(&transforms);
    if (weighted_reloaded) geBody_Destroy(&weighted_reloaded);
    if (weighted) geBody_Destroy(&weighted);
    if (rigid_reloaded) geBody_Destroy(&rigid_reloaded);
    if (rigid) geBody_Destroy(&rigid);
    geVFile_CloseAPI();
    unlink("rigid-a.body"); unlink("rigid-b.body"); unlink("weighted.body");
    if (chdir("/") == 0) rmdir(directory);
    return result;
}
