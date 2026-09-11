/****************************************************************************************/
/*  OglMisc.c                                                                           */
/*                                                                                      */
/*  Author: George McBay (gfm@my-deja.com)                                              */
/*  Description: Miscellaneous support functions for OpenGL driver                      */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D Public License               */
/*  Version 1.01 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                            */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*                                                                                      */
/****************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <GL/gl.h>

#include "Dcommon.h"
#include "OglMisc.h"

/* Keep the colour under keyed texels meaningful before GLU constructs mipmaps.
 * A breadth-first fill copies the nearest opaque edge colour into RGB only;
 * alpha is copied verbatim and therefore retains the source transparency. */
geBoolean Ogl_BleedTransparentRgb32(GLubyte *dstPtr, const GLubyte *srcPtr,
	GLint width, GLint height)
{
	size_t pixelCount, byteCount, head = 0, tail = 0, i;
	size_t *queue;
	unsigned char *visited;

	if (!dstPtr || !srcPtr || width <= 0 || height <= 0)
		return GE_FALSE;
	if ((size_t)width > SIZE_MAX / (size_t)height)
		return GE_FALSE;
	pixelCount = (size_t)width * (size_t)height;
	if (pixelCount > SIZE_MAX / 4 || pixelCount > SIZE_MAX / sizeof(*queue))
		return GE_FALSE;
	byteCount = pixelCount * 4;
	queue = (size_t *)malloc(pixelCount * sizeof(*queue));
	visited = (unsigned char *)calloc(pixelCount, 1);
	if (!queue || !visited) {
		free(queue);
		free(visited);
		return GE_FALSE;
	}
	memcpy(dstPtr, srcPtr, byteCount);
	for (i = 0; i < pixelCount; ++i) {
		if (srcPtr[i * 4 + 3] != 0) {
			visited[i] = 1;
			queue[tail++] = i;
		}
	}
	while (head < tail) {
		const size_t current = queue[head++];
		const size_t x = current % (size_t)width;
		const size_t y = current / (size_t)width;
		size_t neighbours[4], count = 0, n;
		if (x > 0) neighbours[count++] = current - 1;
		if (x + 1 < (size_t)width) neighbours[count++] = current + 1;
		if (y > 0) neighbours[count++] = current - (size_t)width;
		if (y + 1 < (size_t)height) neighbours[count++] = current + (size_t)width;
		for (n = 0; n < count; ++n) {
			const size_t next = neighbours[n];
			if (visited[next]) continue;
			visited[next] = 1;
			dstPtr[next * 4] = dstPtr[current * 4];
			dstPtr[next * 4 + 1] = dstPtr[current * 4 + 1];
			dstPtr[next * 4 + 2] = dstPtr[current * 4 + 2];
			queue[tail++] = next;
		}
	}
	free(visited);
	free(queue);
	return GE_TRUE;
}


// Set up the OpenGL viewing frustum using an orthographic projection (as Genesis does all of
// its transforms in the engine... We're just here to rasterize polygons).
void InitMatrices(int width, int height)
{
	glMatrixMode(GL_PROJECTION);
 	glLoadIdentity();
 	glOrtho(0.0f, (GLfloat)width, 0.0f, (GLfloat)height, 0.0f, 1.0f);
 	glMatrixMode(GL_MODELVIEW);
 	glLoadIdentity();
 	glViewport(0, 0, width, height);

	glScalef(1.0f, -1.0f, 1.0f);
	glTranslatef(0, 0 - (GLfloat)height, 0.0);
}


// Checks to see if a specific OpenGL extention is present.  Used to look for multitexture 
// support.
geBoolean ExtensionExists(const char *extension)
{
	const GLubyte *extensions = NULL;
	const GLubyte *begin;
	GLubyte *cursor, *end;

	cursor = (GLubyte *) strchr(extension, ' ');

	if(cursor || *extension == '\0')
	{
		return GE_FALSE;
	}

	extensions = glGetString(GL_EXTENSIONS);

	begin = extensions;
	
	while(GE_TRUE) 
	{
		cursor = (GLubyte *) strstr((const char *) begin, extension);

		if(!cursor)
		{
			break;
		}

		end = cursor + strlen(extension);

		if(cursor == begin || *(cursor - 1) == ' ')
		{
			if (*end == ' ' || *end == '\0')
			{
				return GE_TRUE;
			}
		}
		
		begin = end;
	}

	return GE_FALSE;
}


// Takes a GE_PIXELFORMAT_24BIT_RGB bitmap and converts it to a GE_PIXELFORMAT_32BIT_ABGR,
// replacing colorkey pixels with alpha information.
void CkBlit24_32(GLubyte *dstPtr, GLint dstWidth, GLint dstHeight, GLubyte *srcPtr, GLint srcWidth, 
				 GLint srcHeight)
{
	GLint width, height;
	GLubyte *nextLine;

	memset(dstPtr, 0x00, dstWidth * dstHeight * 4);


	for(height = 0; height < srcHeight; height++)
	{
		nextLine = (dstPtr + (dstWidth * 4));

		for(width = 0; width < srcWidth; width++)
		{
			if(!(*srcPtr == 0x00 && *(srcPtr + 1) == 0x00 && *(srcPtr + 2) == 0x01))
			{
				*dstPtr = *srcPtr;
				*(dstPtr + 1) = *(srcPtr + 1);
				*(dstPtr + 2) = *(srcPtr + 2);
				*(dstPtr + 3) = 0xFF;
			}

			srcPtr += 3;
			dstPtr += 4;
		}

		dstPtr = nextLine;
	}
}


void Blit32(GLubyte *dstPtr, GLint dstPitch, GLubyte *srcPtr, GLint srcWidth, GLint srcHeight,
			GLint srcPitch)
{
	GLint count;

	for(count = 0; count < srcHeight; count++)
	{
		memcpy(dstPtr, srcPtr, srcWidth * 4);
		srcPtr += srcPitch * 4;
		dstPtr += dstPitch * 4;
	}
}
