/* palette.c — Extract dominant color palette from an image.
 *
 * Usage:  ./palette input.jpg [colors=8]
 * Output: JSON to stdout with hex colors and frequencies
 *
 * Build: cc -O2 -lm palette.c -o palette
 * Dependencies: stb_image.h, common.h
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { int r, g, b; int count; } color_entry;

int color_cmp(const void *a, const void *b) {
    return ((const color_entry*)b)->count - ((const color_entry*)a)->count;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s input.jpg [max_colors=16]\n", argv[0]);
        return 1;
    }
    const char *input = argv[1];
    int max_colors = argc > 2 ? atoi(argv[2]) : 16;
    if (max_colors < 1 || max_colors > 64) max_colors = 16;

    int w, h, channels;
    unsigned char *img = stbi_load(input, &w, &h, &channels, 3);
    if (!img) { fprintf(stderr, "Error: cannot load '%s'\n", input); return 1; }

    int n = w * h;

    /* Quantize to 32-step palette and count */
    /* Use a hash table approach — simpler: just bucket by qcolor */
#define HT_SIZE 32768
    color_entry *ht = calloc((size_t)HT_SIZE, sizeof(color_entry));
    int ht_count = 0;

    for (int i = 0; i < n; i++) {
        int r = img[i*3]   / 32;
        int g = img[i*3+1] / 32;
        int b = img[i*3+2] / 32;
        int hash = (r * 41 + g * 37 + b * 31) & (HT_SIZE - 1);

        /* Linear probe */
        while (ht[hash].count > 0) {
            if (ht[hash].r == r && ht[hash].g == g && ht[hash].b == b) {
                ht[hash].count++;
                goto next_pixel;
            }
            hash = (hash + 1) & (HT_SIZE - 1);
        }
        ht[hash].r = r; ht[hash].g = g; ht[hash].b = b;
        ht[hash].count = 1;
        ht_count++;
next_pixel:;
    }

    /* Collect non-empty entries */
    color_entry *pal = malloc((size_t)ht_count * sizeof(color_entry));
    int pal_n = 0;
    for (int i = 0; i < HT_SIZE; i++)
        if (ht[i].count > 0)
            pal[pal_n++] = ht[i];
    free(ht);

    /* Sort by frequency */
    qsort(pal, (size_t)pal_n, sizeof(color_entry), color_cmp);

    /* Output JSON */
    int out_n = pal_n < max_colors ? pal_n : max_colors;
    printf("{\n  \"image\": \"%s\",\n  \"dimensions\": [%d, %d],\n  \"total_pixels\": %d,\n",
           input, w, h, n);
    printf("  \"colors\": [\n");
    for (int i = 0; i < out_n; i++) {
        int r = pal[i].r * 32 + 16; /* center of quantization bucket */
        int g = pal[i].g * 32 + 16;
        int b = pal[i].b * 32 + 16;
        if (r > 255) { r = 255; }
        if (g > 255) { g = 255; }
        if (b > 255) { b = 255; }
        double pct = 100.0 * pal[i].count / n;
        int warmth = rgb_warmth((uint8_t)r, (uint8_t)g, (uint8_t)b);
        printf("    {\"rgb\":[%d,%d,%d],\"hex\":\"#%02x%02x%02x\",\"pct\":%.1f,\"warmth\":%d}",
               r, g, b, r, g, b, pct, warmth);
        if (i < out_n - 1) printf(",");
        printf("\n");
    }
    printf("  ]\n}\n");

    free(pal);
    stbi_image_free(img);
    return 0;
}
