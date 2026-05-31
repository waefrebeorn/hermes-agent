/* morph.c — Image morphology: edge detection, silhouette mask, difference maps.
 *
 * Usage:  ./morph input.jpg [operation] [output.png]
 * Operations:
 *   edges      — Sobel edge detection → grayscale PNG
 *   mask       — Otsu threshold + flood-fill → binary mask PNG
 *   silhouette — Mask with subject pixels only → grayscale PNG
 *   diff       — Difference from uniform gray → PNG
 *
 * Build: cc -O2 -lm morph.c -o morph
 * Dependencies: stb_image.h for reading, common.h
 *
 * Note: PNG write uses a minimal embedded writer (no stb_image_write dep).
 * Output is ASCII PGM format which can be viewed by any image viewer.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ── Sobel edge detection ── */
static void sobel_edges(uint8_t *gray, uint8_t *out, int w, int h) {
    static const int gx[9] = {-1,0,1, -2,0,2, -1,0,1};
    static const int gy[9] = {-1,-2,-1, 0,0,0, 1,2,1};

    for (int y = 1; y < h-1; y++) {
        for (int x = 1; x < w-1; x++) {
            int sx = 0, sy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int v = gray[(y+ky)*w + (x+kx)];
                    int ki = (ky+1)*3 + (kx+1);
                    sx += v * gx[ki];
                    sy += v * gy[ki];
                }
            }
            int mag = (int)sqrt((double)(sx*sx + sy*sy));
            if (mag > 255) mag = 255;
            out[y*w + x] = (uint8_t)mag;
        }
    }
}

/* ── Write PGM (portable graymap) — no external dep ── */
static int write_pgm(const char *path, uint8_t *pixels, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P5\n%d %d\n255\n", w, h);
    fwrite(pixels, 1, (size_t)w * h, fp);
    fclose(fp);
    return 0;
}

/* ── Otsu + flood fill mask ── */
static int otsu_threshold(uint8_t *gray, int n) {
    int hist[256] = {0};
    for (int i = 0; i < n; i++) hist[gray[i]]++;
    int total = n;
    long sum_total = 0;
    for (int i = 0; i < 256; i++) sum_total += (long)i * hist[i];
    long sum_bg = 0;
    int w_bg = 0;
    double max_var = 0;
    int thresh = 128;
    for (int t = 0; t < 256; t++) {
        w_bg += hist[t];
        if (w_bg == 0 || w_bg == total) continue;
        int w_fg = total - w_bg;
        sum_bg += (long)t * hist[t];
        double mean_bg = (double)sum_bg / w_bg;
        double mean_fg = (double)(sum_total - sum_bg) / w_fg;
        double var = (double)w_bg * w_fg * (mean_bg - mean_fg) * (mean_bg - mean_fg);
        if (var > max_var) { max_var = var; thresh = t; }
    }
    return thresh;
}

static void flood_fill_mask(uint8_t *gray, uint8_t *mask, int w, int h, int thresh, int invert) {
    memset(mask, 0, (size_t)w * h);
    int cap = w * h;
    int *sx = malloc((size_t)cap * sizeof(int));
    int *sy = malloc((size_t)cap * sizeof(int));
    int sp = 0;
    sx[sp] = w/2; sy[sp] = h/2; sp++;

    while (sp > 0) {
        int x = sx[--sp], y = sy[sp];
        if (x < 0 || x >= w || y < 0 || y >= h || mask[y*w + x]) continue;
        int pass = invert ? (gray[y*w + x] < thresh) : (gray[y*w + x] > thresh);
        if (!pass) continue;
        mask[y*w + x] = 255;
        if (sp + 4 > cap) { cap *= 2;
            sx = realloc(sx, (size_t)cap * sizeof(int));
            sy = realloc(sy, (size_t)cap * sizeof(int)); }
        sx[sp]=x+1; sy[sp]=y; sp++;
        sx[sp]=x-1; sy[sp]=y; sp++;
        sx[sp]=x; sy[sp]=y+1; sp++;
        sx[sp]=x; sy[sp]=y-1; sp++;
    }
    free(sx); free(sy);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.jpg <operation> [output.pgm]\n", argv[0]);
        fprintf(stderr, "Operations:\n");
        fprintf(stderr, "  edges       — Sobel edge detection\n");
        fprintf(stderr, "  mask        — Binary silhouette mask\n");
        fprintf(stderr, "  silhouette  — Subject pixels on black bg\n");
        fprintf(stderr, "  diff        — Deviation from mean gray\n");
        return 1;
    }

    const char *input = argv[1];
    const char *op = argv[2];
    char out_path[1024];
    if (argc > 3) snprintf(out_path, sizeof(out_path), "%s", argv[3]);
    else          snprintf(out_path, sizeof(out_path), "morph_%s.pgm", op);

    int w, h, channels;
    unsigned char *img = stbi_load(input, &w, &h, &channels, 3);
    if (!img) { fprintf(stderr, "Error: cannot load '%s'\n", input); return 1; }

    uint8_t *gray = malloc((size_t)w * h);
    uint8_t *out  = calloc((size_t)w * h, 1);
    for (int i = 0; i < w * h; i++)
        gray[i] = rgb_luminance(img[i*3], img[i*3+1], img[i*3+2]);

    if (strcmp(op, "edges") == 0) {
        sobel_edges(gray, out, w, h);
        write_pgm(out_path, out, w, h);
        printf("Edge detection: wrote %s (%dx%d)\n", out_path, w, h);
    }
    else if (strcmp(op, "mask") == 0) {
        int thresh = otsu_threshold(gray, w * h);
        flood_fill_mask(gray, out, w, h, thresh, 0);
        write_pgm(out_path, out, w, h);
        printf("Mask: wrote %s (threshold=%d)\n", out_path, thresh);
    }
    else if (strcmp(op, "silhouette") == 0) {
        int thresh = otsu_threshold(gray, w * h);
        uint8_t *mask = malloc((size_t)w * h);
        flood_fill_mask(gray, mask, w, h, thresh, 0);
        for (int i = 0; i < w * h; i++)
            out[i] = mask[i] > 128 ? gray[i] : 0;
        write_pgm(out_path, out, w, h);
        free(mask);
        printf("Silhouette: wrote %s\n", out_path);
    }
    else if (strcmp(op, "diff") == 0) {
        long sum = 0;
        for (int i = 0; i < w * h; i++) sum += gray[i];
        int mean = (int)(sum / (w * h));
        for (int i = 0; i < w * h; i++) {
            int d = abs(gray[i] - mean) * 2;
            out[i] = d > 255 ? 255 : (uint8_t)d;
        }
        write_pgm(out_path, out, w, h);
        printf("Difference map: wrote %s (mean=%d)\n", out_path, mean);
    }
    else {
        fprintf(stderr, "Unknown operation: %s\n", op);
        fprintf(stderr, "Try: edges, mask, silhouette, diff\n");
        return 1;
    }

    free(gray); free(out);
    stbi_image_free(img);
    return 0;
}
