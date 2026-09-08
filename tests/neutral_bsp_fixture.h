/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Synthetic GBSP v15 with one lightmapped wall.
 */
#ifndef NEUTRAL_BSP_FIXTURE_H
#define NEUTRAL_BSP_FIXTURE_H
#include "Gbspfile.h"
#include <stddef.h>
#include <string.h>

static int fixture_chunk(geVFile *file, int type, int size, int count, const void *data)
{
    GBSP_Chunk chunk = {type, size, count};
    return geVFile_Write(file, &chunk, sizeof(chunk)) &&
           (!count || geVFile_Write(file, data, size * count));
}

static int neutral_bsp_write(geVFile *file)
{
    GBSP_Header header = {{'G','B','S','P',0}, GBSP_VERSION, {0}};
    GFX_Model model = {0};
    unsigned char disk_model[80] = {0};
    GFX_Node node = {0};
    GFX_Leaf leaves[2] = {{0}};
    GFX_Cluster cluster = {-1};
    GFX_Area areas[2] = {{0}};
    GFX_Plane plane = {{0,0,1}, -100, PLANE_Z};
    GFX_Face face = {0};
    GFX_TexInfo texinfo = {0};
    GFX_Texture texture = {"NEUTRAL_GREEN",0,16,16,0,0};
    DRV_Palette palette = {{0}};
    GFX_SkyData sky = {0};
    int32 indices[4] = {0,1,2,3}, leaf_face = 0;
    geVec3d verts[4] = {{-48,48,-100},{48,48,-100},
                        {48,-48,-100},{-48,-48,-100}};
    geVec3d rgb[4] = {{255,255,255},{255,255,255},
                      {255,255,255},{255,255,255}};
    unsigned char pixels[1+16*16*3], light[1+7*7*3];
    int result = 0;
    int i;
    model.RootNode[0] = 0;
    model.RootNode[1] = -1;
    model.Mins = (geVec3d){-128,-128,-128};
    model.Maxs = (geVec3d){128,128,128};
    model.NumFaces = 1; model.NumLeafs = 2; model.NumClusters = 1;
    /* The disk model has a 32-bit motion placeholder; host pointers never
     * enter the fixture stream. The first 76 bytes are fixed-width fields. */
    _Static_assert(offsetof(GFX_Model, Areas) == 68, "model prefix changed");
    memcpy(disk_model, &model, 76);
    node.Children[0] = -1; node.Children[1] = -2;
    node.NumFaces = 1; node.Mins = model.Mins; node.Maxs = model.Maxs;
    leaves[0].Mins = leaves[1].Mins = model.Mins;
    leaves[0].Maxs = leaves[1].Maxs = model.Maxs;
    leaves[0].NumFaces = 1; leaves[0].Area = 1;
    leaves[1].Contents = GE_CONTENTS_SOLID; leaves[1].Cluster = -1;
    face.NumVerts = 4; face.LWidth = 7; face.LHeight = 7;
    face.LTypes[1] = face.LTypes[2] = face.LTypes[3] = 255;
    texinfo.Vecs[0].X = 1; texinfo.Vecs[1].Y = 1;
    texinfo.DrawScale[0] = texinfo.DrawScale[1] = 1;
    texinfo.Alpha = 255;
    for (i = 0; i < 6; ++i) sky.Textures[i] = -1;
    sky.DrawScale = 1;
    pixels[0] = GE_PIXELFORMAT_24BIT_RGB;
    for (i = 0; i < 16*16; ++i) {
        pixels[1+i*3] = 24; pixels[2+i*3] = 220; pixels[3+i*3] = 48;
    }
    memset(light, 128, sizeof(light)); light[0] = 1;
    if (!file) return 0;
#define CHUNK(type, data, count) \
    if (!fixture_chunk(file, type, sizeof(*(data)), count, data)) goto done
    CHUNK(GBSP_CHUNK_HEADER, &header, 1);
    if (!fixture_chunk(file, GBSP_CHUNK_MODELS, 80, 1, disk_model)) goto done;
    CHUNK(GBSP_CHUNK_NODES, &node, 1);
    CHUNK(GBSP_CHUNK_LEAFS, leaves, 2);
    CHUNK(GBSP_CHUNK_CLUSTERS, &cluster, 1);
    CHUNK(GBSP_CHUNK_AREAS, areas, 2);
    CHUNK(GBSP_CHUNK_PLANES, &plane, 1);
    CHUNK(GBSP_CHUNK_FACES, &face, 1);
    CHUNK(GBSP_CHUNK_LEAF_FACES, &leaf_face, 1);
    CHUNK(GBSP_CHUNK_VERT_INDEX, indices, 4);
    CHUNK(GBSP_CHUNK_VERTS, verts, 4);
    CHUNK(GBSP_CHUNK_RGB_VERTS, rgb, 4);
    CHUNK(GBSP_CHUNK_TEXINFO, &texinfo, 1);
    CHUNK(GBSP_CHUNK_TEXTURES, &texture, 1);
    CHUNK(GBSP_CHUNK_TEXDATA, pixels, sizeof(pixels));
    CHUNK(GBSP_CHUNK_PALETTES, &palette, 1);
    CHUNK(GBSP_CHUNK_LIGHTDATA, light, sizeof(light));
    CHUNK(GBSP_CHUNK_SKYDATA, &sky, 1);
#undef CHUNK
    result = fixture_chunk(file, GBSP_CHUNK_END, 0, 0, NULL);
done:
    return result;
}

static geWorld *neutral_bsp_world(void)
{
    geVFile_MemoryContext context = {0};
    geVFile *file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_MEMORY, NULL,
                                         &context, GE_VFILE_OPEN_CREATE);
    geWorld *world = NULL;
    if (!file) return NULL;
    if (neutral_bsp_write(file) && geVFile_Seek(file, 0, GE_VFILE_SEEKSET))
        world = geWorld_Create(file);
    geVFile_Close(file);
    return world;
}
#endif
