/* asciimg.c — Convert an image to ASCII art.
 *
 * Usage:  ./asciimg input.jpg [width=80] [charset=std]
 *   charset: std (@%#*+=-:. ), block (█▓▒░ ), or shade (█▛▜▟▙ )
 * Output: ASCII art to stdout
 *
 * Build: cc -O2 -lm asciimg.c -o asciimg
 * Dependencies: stb_image.h, common.h
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *charsets[] = {
    "@%%#*+=-:. ",       /* std */
    "█▓▒░ ",             /* block */
    "█▛▜▟▙▀▄▌▐ ",       /* shade */
    "#@%&8BMWwmqbdpqdb0OoQDXdoxzcvunxrjft/|()1{}[]?Il!i>:;,.  ", /* detail */
    NULL
};

static const char *charset_names[] = {"std", "block", "shade", "detail", NULL};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s input.jpg [width=80] [charset=std|block|shade|detail]\n", argv[0]);
        fprintf(stderr, "Output to stdout; redirect to file for saving.\n");
        fprintf(stderr, "  --invert    Invert (dark bg, light fg)\n");
        fprintf(stderr, "  --color     Output ANSI 256-color (requires --color)\n");
        return 1;
    }

    const char *input = argv[1];
    int out_w = argc > 2 ? atoi(argv[2]) : 80;
    if (out_w < 10) out_w = 10;
    if (out_w > 200) out_w = 200;

    const char *cs = "std";
    int invert = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--invert") == 0) invert = 1;
        else if (strcmp(argv[i], "--color") == 0) { /* handled below */ }
        else cs = argv[i];
    }

    int w, h, channels;
    unsigned char *img = stbi_load(input, &w, &h, &channels, 3);
    if (!img) { fprintf(stderr, "Error: cannot load '%s'\n", input); return 1; }

    /* Find charset */
    const char *chars = NULL;
    for (int i = 0; charsets[i]; i++) {
        if (strcmp(cs, charset_names[i]) == 0) { chars = charsets[i]; break; }
    }
    if (!chars) { chars = charsets[0]; } /* default to std */

    int clen = (int)strlen(chars);

    /* Determine output height (maintain aspect, ~2:1 char aspect ratio) */
    double aspect = (double)h / w * 0.5;
    int out_h = (int)(out_w * aspect);
    if (out_h < 1) out_h = 1;

    /* Scale image */
    /* We'll sample pixel blocks */
    double x_step = (double)w / out_w;
    double y_step = (double)h / out_h;

    for (int y = 0; y < out_h; y++) {
        for (int x = 0; x < out_w; x++) {
            /* Average pixel block */
            int sx = (int)(x * x_step), sy = (int)(y * y_step);
            int ex = (int)((x + 1) * x_step), ey = (int)((y + 1) * y_step);
            if (ex > w) { ex = w; }
            if (ey > h) { ey = h; }

            int r_sum = 0, g_sum = 0, b_sum = 0, pc = 0;
            for (int py = sy; py < ey; py++) {
                for (int px = sx; px < ex; px++) {
                    int idx = (py * w + px) * 3;
                    r_sum += img[idx]; g_sum += img[idx+1]; b_sum += img[idx+2];
                    pc++;
                }
            }
            if (pc == 0) pc = 1;
            int r = r_sum / pc, g = g_sum / pc, b = b_sum / pc;
            int lum = rgb_luminance((uint8_t)r, (uint8_t)g, (uint8_t)b);

            int idx = invert ? (255 - lum) : lum;
            int ci = idx * clen / 256;
            if (ci >= clen) ci = clen - 1;
            putchar(chars[ci]);
        }
        putchar('\n');
    }

    stbi_image_free(img);
    return 0;
}
