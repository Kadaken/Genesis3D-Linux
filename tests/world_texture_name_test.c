#include "Genesis.h"
#include "World.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	geWorld World;
	World_BSP Bsp;
	GFX_Model Model;
	GFX_Leaf Leaf;
	GFX_Face Faces[2];
	GFX_Plane Planes[2];
	GFX_TexInfo TexInfo[2];
	GFX_Texture Textures[2];
	geVec3d Vertices[8] = {
		{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f},
		{1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f},
		{0.01f, 10.0f, 10.0f}, {0.01f, 20.0f, 10.0f},
		{0.01f, 20.0f, 20.0f}, {0.01f, 10.0f, 20.0f}
	};
	int32 VertexIndices[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	int32 LeafFaces[2] = {0, 1};
	geVec3d Position = {0.0f, 0.05f, 0.0f};
	geVec3d Extents = {0.0f, 0.0f, 0.0f};
	char TextureName[32];
	GE_Collision Collision;

	memset(&World, 0, sizeof(World));
	memset(&Bsp, 0, sizeof(Bsp));
	memset(&Model, 0, sizeof(Model));
	memset(&Leaf, 0, sizeof(Leaf));
	memset(Faces, 0, sizeof(Faces));
	memset(Planes, 0, sizeof(Planes));
	memset(TexInfo, 0, sizeof(TexInfo));
	memset(Textures, 0, sizeof(Textures));

	strcpy(Textures[0].Name, "NEUTRAL_FLOOR");
	strcpy(Textures[1].Name, "NEUTRAL_WALL");
	Planes[0].Normal.Y = 1.0f;
	Planes[1].Normal.X = 1.0f;
	Planes[1].Dist = 0.01f;
	Faces[0].FirstVert = 0;
	Faces[0].NumVerts = 4;
	Faces[0].PlaneNum = 0;
	Faces[0].TexInfo = 0;
	Faces[1].FirstVert = 4;
	Faces[1].NumVerts = 4;
	Faces[1].PlaneNum = 1;
	Faces[1].TexInfo = 1;
	TexInfo[0].Texture = 0;
	TexInfo[1].Texture = 1;
	Leaf.FirstFace = 0;
	Leaf.NumFaces = 2;
	Model.RootNode[0] = -1;
	geXForm3d_SetIdentity(&Bsp.Models[0].XForm);

	Bsp.BSPData.GFXModels = &Model;
	Bsp.BSPData.GFXLeafs = &Leaf;
	Bsp.BSPData.GFXPlanes = Planes;
	Bsp.BSPData.GFXFaces = Faces;
	Bsp.BSPData.GFXLeafFaces = LeafFaces;
	Bsp.BSPData.GFXVerts = Vertices;
	Bsp.BSPData.GFXVertIndexList = VertexIndices;
	Bsp.BSPData.GFXTexInfo = TexInfo;
	Bsp.BSPData.GFXTextures = Textures;
	Bsp.BSPData.NumGFXModels = 1;
	Bsp.BSPData.NumGFXLeafs = 1;
	Bsp.BSPData.NumGFXPlanes = 2;
	Bsp.BSPData.NumGFXFaces = 2;
	Bsp.BSPData.NumGFXLeafFaces = 2;
	Bsp.BSPData.NumGFXVerts = 8;
	Bsp.BSPData.NumGFXVertIndexList = 8;
	Bsp.BSPData.NumGFXTexInfo = 2;
	Bsp.BSPData.NumGFXTextures = 2;
	World.CurrentBSP = &Bsp;
	Bsp.Models[0].GFXModelNum = 0;
	Model.FirstFace = 0;
	Model.NumFaces = 2;

	if (geWorld_GetTextureName(&World, &Position, &Extents, &Extents,
		TextureName) != GE_TRUE)
	{
		fprintf(stderr, "texture fixture: lookup returned no surface\n");
		return 1;
	}
	if (strcmp(TextureName, "NEUTRAL_FLOOR") != 0)
	{
		fprintf(stderr, "texture fixture: unexpected material name\n");
		return 1;
	}

	memset(&Collision, 0, sizeof(Collision));
	Collision.Model = &Bsp.Models[0];
	Collision.Impact = Position;
	Collision.Plane.Normal = Planes[0].Normal;
	Collision.Plane.Dist = Planes[0].Dist;
	if (geWorld_GetTextureNameFromCollision(&World, &Collision,
		TextureName) != GE_TRUE || strcmp(TextureName, "NEUTRAL_FLOOR") != 0)
	{
		fprintf(stderr, "texture fixture: collision selected wrong material\n");
		return 1;
	}
	return 0;
}
