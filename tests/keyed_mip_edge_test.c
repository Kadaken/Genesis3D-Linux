/* Genesis3D Public License v1.01; see ../g3dlicense.txt.
 * Contributor: Kadaken, 2026. Keyed-texture mip edge regression.
 */
#include "Dcommon.h"
#include <GL/gl.h>
#include "OglMisc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    GLubyte source[4 * 4 * 4] = {0};
    GLubyte result[sizeof(source)];
    unsigned int sum[3] = {0, 0, 0};
    const GLubyte edge[3] = {241, 113, 19};
    int i, c;

    /* One deliberately asymmetric opaque texel surrounded by keyed texels.
     * A box-filtered 1x1 mip must retain its edge colour instead of mixing in
     * black RGB from alpha-zero source texels. */
    memcpy(source + (5 * 4), edge, 3);
    source[5 * 4 + 3] = 255;
    if (!Ogl_BleedTransparentRgb32(result, source, 4, 4)) return 1;
    if (memcmp(source + (5 * 4), edge, 3) != 0 || source[0] != 0) return 2;
    for (i = 0; i < 16; ++i) {
        if (result[i * 4 + 3] != source[i * 4 + 3]) return 3;
        for (c = 0; c < 3; ++c) sum[c] += result[i * 4 + c];
    }
    for (c = 0; c < 3; ++c) {
        const unsigned int minified = (sum[c] + 8) / 16;
        if (minified != edge[c]) {
            fprintf(stderr, "keyed mip edge: channel=%d expected=%u got=%u\n",
                    c, edge[c], minified);
            return 4;
        }
    }
    puts("keyed mip edge fixture: PASS");
    return 0;
}
