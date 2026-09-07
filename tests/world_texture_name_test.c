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
	GFX_Face Face;
	GFX_Plane Plane;
	GFX_TexInfo TexInfo;
	GFX_Texture Texture;
	int32 LeafFace = 0;
	geVec3d Position = {0.0f, 0.0f, 0.0f};
	geVec3d Extents = {0.0f, 0.0f, 0.0f};
	char TextureName[32];

	memset(&World, 0, sizeof(World));
	memset(&Bsp, 0, sizeof(Bsp));
	memset(&Model, 0, sizeof(Model));
	memset(&Leaf, 0, sizeof(Leaf));
	memset(&Face, 0, sizeof(Face));
	memset(&Plane, 0, sizeof(Plane));
	memset(&TexInfo, 0, sizeof(TexInfo));
	memset(&Texture, 0, sizeof(Texture));

	strcpy(Texture.Name, "NEUTRAL_FLOOR");
	Plane.Normal.Y = 1.0f;
	Face.PlaneNum = 0;
	Face.TexInfo = 0;
	TexInfo.Texture = 0;
	Leaf.FirstFace = 0;
	Leaf.NumFaces = 1;
	Model.RootNode[0] = -1;
	geXForm3d_SetIdentity(&Bsp.Models[0].XForm);

	Bsp.BSPData.GFXModels = &Model;
	Bsp.BSPData.GFXLeafs = &Leaf;
	Bsp.BSPData.GFXPlanes = &Plane;
	Bsp.BSPData.GFXFaces = &Face;
	Bsp.BSPData.GFXLeafFaces = &LeafFace;
	Bsp.BSPData.GFXTexInfo = &TexInfo;
	Bsp.BSPData.GFXTextures = &Texture;
	Bsp.BSPData.NumGFXModels = 1;
	Bsp.BSPData.NumGFXLeafs = 1;
	Bsp.BSPData.NumGFXPlanes = 1;
	Bsp.BSPData.NumGFXFaces = 1;
	Bsp.BSPData.NumGFXLeafFaces = 1;
	Bsp.BSPData.NumGFXTexInfo = 1;
	Bsp.BSPData.NumGFXTextures = 1;
	World.CurrentBSP = &Bsp;

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
	return 0;
}
