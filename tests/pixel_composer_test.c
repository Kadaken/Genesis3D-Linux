/* Public packed-pixel API; no assets, SDL or graphics context required. */
#include "Genesis.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const struct {
    gePixelFormat format;
    const char *order;
} formats[] = {
    {GE_PIXELFORMAT_32BIT_RGBX, "RGBX"},
    {GE_PIXELFORMAT_32BIT_BGRX, "BGRX"},
    {GE_PIXELFORMAT_32BIT_RGBA, "RGBA"},
    {GE_PIXELFORMAT_32BIT_ARGB, "ARGB"},
    {GE_PIXELFORMAT_32BIT_BGRA, "BGRA"},
    {GE_PIXELFORMAT_32BIT_ABGR, "ABGR"}
};

int main(int argc, char **argv)
{
    const int values[] = {0, 127, 128, 182, 255};
    unsigned f, ri, gi, bi, ai, total = 0;
    if (argc > 2) return 2;
    for (f = 0; f < sizeof(formats)/sizeof(formats[0]); ++f) {
        unsigned cases = 0;
        if (argc == 2 && strcmp(argv[1], formats[f].order)) continue;
        for (ri=0; ri<5; ++ri) for (gi=0; gi<5; ++gi)
        for (bi=0; bi<5; ++bi) for (ai=0; ai<5; ++ai) {
            int r = values[ri], g = values[gi], b = values[bi], a = values[ai];
            int out_r = -1, out_g = -1, out_b = -1, out_a = -1;
            uint32 pixel;
            uint64_t expected = 0;
            unsigned slot;
            /* Independent byte-order oracle, not the composer's shift formula. */
            for (slot=0; slot<4; ++slot) {
                char channel = formats[f].order[slot];
                int value = channel == 'R' ? r : channel == 'G' ? g :
                    channel == 'B' ? b : channel == 'A' ? a : 0;
                expected = expected * 256 + (unsigned)value;
            }
            pixel = gePixelFormat_ComposePixel(formats[f].format, r, g, b, a);
            gePixelFormat_DecomposePixel(formats[f].format, pixel, &out_r, &out_g, &out_b, &out_a);
            if (pixel != expected || out_r != r || out_g != g || out_b != b ||
                out_a != (formats[f].order[3] == 'X' ? 255 : a)) {
                fprintf(stderr, "pixel composer %s: input=%d/%d/%d/%d pixel=%08x expected=%08x output=%d/%d/%d/%d\n",
                    formats[f].order, r, g, b, a, (unsigned)pixel, (unsigned)expected,
                    out_r, out_g, out_b, out_a);
                return 1;
            }
            ++cases;
        }
        total += cases;
        fprintf(stderr, "pixel composer %s: %u combinations passed\n", formats[f].order, cases);
    }
    if (!total) return 2;
    fprintf(stderr, "pixel composers: %u total combinations passed\n", total);
    return 0;
}
