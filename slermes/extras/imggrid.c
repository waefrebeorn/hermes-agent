/* imggrid.c — Convert image to grid-based SVG (pixel-art style).
 * Detects dominant color per grid cell for multi-colored subjects.
 *
 * Usage:  ./imggrid input.jpg output.svg [cols=20] [rows=26] [scale=10]
 *   --nobg    Transparent background (don't fill dark cells)
 *   --invert  Invert threshold
 *
 * Build: cc -O2 -lm imggrid.c -o imggrid
 * Dependencies: stb_image.h
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simple hash map for quantized color counting */
#define HT_SIZE 4096
typedef struct { int r, g, b; int count; int r_sum, g_sum, b_sum; } bucket;

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.jpg output.svg [cols=20] [rows=26] [scale=10]\n", argv[0]);
        fprintf(stderr, "  --nobg    Transparent background (default: dark bg)\n");
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];
    int cols = argc > 3 ? atoi(argv[3]) : 20;
    int rows = argc > 4 ? atoi(argv[4]) : 26;
    int scale = argc > 5 ? atoi(argv[5]) : 10;
    int dark_bg = 1;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--nobg") == 0) dark_bg = 0;
    }

    if (cols < 4) cols = 4;
    if (rows < 4) rows = 4;
    if (scale < 2) scale = 2;

    int w, h, channels;
    unsigned char *img = stbi_load(input, &w, &h, &channels, 3);
    if (!img) { fprintf(stderr, "Error: cannot load '%s'\n", input); return 1; }

    int cell_w = w / cols;
    int cell_h = h / rows;
    int svg_w = cols * scale;
    int svg_h = rows * scale;

    /* Buffer for SVG */
    size_t svg_cap = (size_t)cols * rows * 80 + 1024;
    char *svg = malloc(svg_cap);
    size_t pos = 0;

    pos += snprintf(svg + pos, svg_cap - pos,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\">\n",
        svg_w, svg_h, svg_w * 2, svg_h * 2);

    if (dark_bg)
        pos += snprintf(svg + pos, svg_cap - pos,
            "  <rect width=\"100%%\" height=\"100%%\" fill=\"#1a1a2e\"/>\n");

    bucket *ht = calloc((size_t)HT_SIZE, sizeof(bucket));

    for (int row = 0; row < rows; row++) {
        int y1 = row * cell_h;
        int y2 = (row + 1) * cell_h;
        if (y2 > h) y2 = h;

        for (int col = 0; col < cols; col++) {
            int x1 = col * cell_w;
            int x2 = (col + 1) * cell_w;
            if (x2 > w) x2 = w;

            /* Count quantized colors in this cell */
            memset(ht, 0, (size_t)HT_SIZE * sizeof(bucket));

            for (int y = y1; y < y2; y++) {
                for (int x = x1; x < x2; x++) {
                    int idx = (y * w + x) * 3;
                    int r = img[idx], g = img[idx+1], b = img[idx+2];
                    /* 32-step quantization */
                    int qr = r / 32, qg = g / 32, qb = b / 32;
                    int hash = (qr * 41 + qg * 37 + qb * 31) & (HT_SIZE - 1);
                    while (ht[hash].count > 0) {
                        if (ht[hash].r == qr && ht[hash].g == qg && ht[hash].b == qb) {
                            ht[hash].count++;
                            ht[hash].r_sum += r;
                            ht[hash].g_sum += g;
                            ht[hash].b_sum += b;
                            goto next_pixel;
                        }
                        hash = (hash + 1) & (HT_SIZE - 1);
                    }
                    ht[hash].r = qr; ht[hash].g = qg; ht[hash].b = qb;
                    ht[hash].count = 1;
                    ht[hash].r_sum = r;
                    ht[hash].g_sum = g;
                    ht[hash].b_sum = b;
next_pixel:;
                }
            }

            /* Find dominant color */
            int best_idx = -1, best_count = 0;
            for (int i = 0; i < HT_SIZE; i++) {
                if (ht[i].count > best_count) {
                    best_count = ht[i].count;
                    best_idx = i;
                }
            }

            int cr = 128, cg = 128, cb = 128;
            if (best_idx >= 0) {
                cr = ht[best_idx].r_sum / ht[best_idx].count;
                cg = ht[best_idx].g_sum / ht[best_idx].count;
                cb = ht[best_idx].b_sum / ht[best_idx].count;
            }

            /* Skip near-black cells on dark bg */
            if (dark_bg && cr < 15 && cg < 15 && cb < 15)
                continue;

            int x = col * scale;
            int y = row * scale;
            pos += snprintf(svg + pos, svg_cap - pos,
                "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#%02x%02x%02x\"/>\n",
                x, y, scale, scale, cr, cg, cb);
        }
    }

    pos += snprintf(svg + pos, svg_cap - pos, "</svg>\n");

    /* Write */
    FILE *fp = fopen(output, "wb");
    if (!fp) { fprintf(stderr, "Error: cannot write '%s'\n", output); return 1; }
    fwrite(svg, 1, pos, fp);
    fclose(fp);
    printf("Wrote %s (%zu bytes, %dx%d grid)\n", output, pos, cols, rows);

    free(svg); free(ht);
    stbi_image_free(img);
    return 0;
}
