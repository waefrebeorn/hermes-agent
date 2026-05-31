/* img2svg.c — Load an image, trace its contour, output SVG vector.
 *
 * Usage:  ./img2svg input.jpg output.svg [options]
 * Options:
 *   --eps NUM     Douglas-Peucker epsilon (default: 1.0)
 *   --invert      Invert mask (subject darker than background)
 *   --eyes        Add eye/nose dots (default: off)
 *   --crop        Auto-crop to subject bounding box
 *   --grid N      Grid mode: divide into NxM grid, dominant color per cell
 *                 (default: off — uses contour tracing instead)
 *
 * Dependencies: stb_image.h (bundled), common.h
 * Build: cc -O2 -lm img2svg.c -o img2svg
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ── Otsu threshold ── */
static int otsu_threshold(uint8_t *gray, int n) {
    int hist[256] = {0};
    for (int i = 0; i < n; i++) hist[gray[i]]++;
    int total = n;
    long sum_total = 0;
    for (int i = 0; i < 256; i++) sum_total += (long)i * hist[i];
    long sum_bg = 0;
    int w_bg = 0, w_fg;
    double max_var = 0;
    int thresh = 128;
    for (int t = 0; t < 256; t++) {
        w_bg += hist[t];
        if (w_bg == 0 || w_bg == total) continue;
        w_fg = total - w_bg;
        sum_bg += (long)t * hist[t];
        double mean_bg = (double)sum_bg / w_bg;
        double mean_fg = (double)(sum_total - sum_bg) / w_fg;
        double var = (double)w_bg * w_fg * (mean_bg - mean_fg) * (mean_bg - mean_fg);
        if (var > max_var) { max_var = var; thresh = t; }
    }
    return thresh;
}

/* ── Flood-fill from center ── */
static void flood_fill_mask(uint8_t *gray, uint8_t *mask, int w, int h, int thresh, int invert) {
    memset(mask, 0, (size_t)w * h);

    /* Stack-based flood fill from center */
    int cap = w * h;
    int *stack_x = malloc((size_t)cap * sizeof(int));
    int *stack_y = malloc((size_t)cap * sizeof(int));
    int sp = 0;

    int cx = w / 2, cy = h / 2;
    stack_x[sp] = cx; stack_y[sp] = cy; sp++;

    while (sp > 0) {
        int x = stack_x[--sp];
        int y = stack_y[sp];
        if (x < 0 || x >= w || y < 0 || y >= h) continue;
        if (mask[y * w + x]) continue;

        int passes = invert ? (gray[y * w + x] < thresh) : (gray[y * w + x] > thresh);
        if (!passes) continue;

        mask[y * w + x] = 255;
        if (sp + 4 > cap) { cap *= 2; stack_x = realloc(stack_x, (size_t)cap * sizeof(int));
                                     stack_y = realloc(stack_y, (size_t)cap * sizeof(int)); }
        stack_x[sp] = x+1; stack_y[sp] = y; sp++;
        stack_x[sp] = x-1; stack_y[sp] = y; sp++;
        stack_x[sp] = x;   stack_y[sp] = y+1; sp++;
        stack_x[sp] = x;   stack_y[sp] = y-1; sp++;
    }

    free(stack_x); free(stack_y);
}

/* ── Median filter 3x3 ── */
static void median_filter_3(uint8_t *src, uint8_t *dst, int w, int h) {
    for (int y = 1; y < h-1; y++) {
        for (int x = 1; x < w-1; x++) {
            uint8_t buf[9];
            int idx = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    buf[idx++] = src[(y+dy)*w + (x+dx)];
            /* Simple bubble sort of 9 elements */
            for (int i = 0; i < 9; i++)
                for (int j = i+1; j < 9; j++)
                    if (buf[i] > buf[j]) { uint8_t t = buf[i]; buf[i] = buf[j]; buf[j] = t; }
            dst[y*w + x] = buf[4];
        }
    }
    /* Copy edges */
    for (int x = 0; x < w; x++) { dst[x] = src[x]; dst[(h-1)*w + x] = src[(h-1)*w + x]; }
    for (int y = 0; y < h; y++) { dst[y*w] = src[y*w]; dst[y*w + w-1] = src[y*w + w-1]; }
}

/* ── Min/Max filter (erode/dilate) ── */
static void morph_filter(uint8_t *src, uint8_t *dst, int w, int h, int size, int max_mode) {
    int half = size / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t best = max_mode ? 0 : 255;
            for (int dy = -half; dy <= half; dy++) {
                for (int dx = -half; dx <= half; dx++) {
                    int px = x + dx, py = y + dy;
                    if (px < 0) { px = 0; }
                    if (px >= w) { px = w - 1; }
                    if (py < 0) { py = 0; }
                    if (py >= h) { py = h - 1; }
                    uint8_t v = src[py*w + px];
                    if (max_mode) { if (v > best) best = v; }
                    else          { if (v < best) best = v; }
                }
            }
            dst[y*w + x] = best;
        }
    }
}

/* ── Bounding box of mask ── */
static void mask_bbox(uint8_t *mask, int w, int h, int *min_x, int *max_x, int *min_y, int *max_y) {
    *min_x = w; *max_x = 0; *min_y = h; *max_y = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (mask[y*w + x] > 128) {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
}

/* ── Eye detection ── */
static void find_eyes(uint8_t *img_rgb, uint8_t *mask, int w, int h,
                      int bbox[4], int *lx, int *ly, int *rx, int *ry) {
    int min_x = bbox[0], max_x = bbox[1], min_y = bbox[2], max_y = bbox[3];
    int bw = max_x - min_x, bh = max_y - min_y;

    int eye_top    = min_y + (int)(bh * 0.15);
    int eye_bottom = min_y + (int)(bh * 0.45);
    int eye_left   = min_x + (int)(bw * 0.15);
    int eye_right  = min_x + (int)(bw * 0.85);
    int mid_x      = (min_x + max_x) / 2;

    /* Collect dark pixels in eye zone */
    int dark_cap = (eye_bottom - eye_top) * (eye_right - eye_left);
    int *dark_x = malloc((size_t)dark_cap * sizeof(int));
    int *dark_y = malloc((size_t)dark_cap * sizeof(int));
    int dark_n = 0;

    for (int y = eye_top; y < eye_bottom && y < h; y++) {
        for (int x = eye_left; x < eye_right && x < w; x++) {
            if (mask[y*w + x] > 128) {
                int idx = y * w * 3 + x * 3;
                uint8_t lum = rgb_luminance(img_rgb[idx], img_rgb[idx+1], img_rgb[idx+2]);
                if (lum < 80 && dark_n < dark_cap) {
                    dark_x[dark_n] = x; dark_y[dark_n] = y; dark_n++;
                }
            }
        }
    }

    /* Find left and right clusters */
    int n = dark_n < 200 ? dark_n : 200;
    /* Simple: average 50 darkest on each side */
    /* Sort by luminance (already roughly sorted, but just pick first n) */
    int lx_sum = 0, ly_sum = 0, lcnt = 0;
    int rx_sum = 0, ry_sum = 0, rcnt = 0;
    int limit = n < 50 ? n : 50;

    for (int i = 0; i < dark_n && (lcnt < limit || rcnt < limit); i++) {
        if (dark_x[i] < mid_x && lcnt < limit) {
            lx_sum += dark_x[i]; ly_sum += dark_y[i]; lcnt++;
        } else if (dark_x[i] >= mid_x && rcnt < limit) {
            rx_sum += dark_x[i]; ry_sum += dark_y[i]; rcnt++;
        }
    }

    if (lcnt > 0) { *lx = lx_sum / lcnt; *ly = ly_sum / lcnt; }
    else          { *lx = mid_x - bw/5;   *ly = eye_top + bh/3; }

    if (rcnt > 0) { *rx = rx_sum / rcnt; *ry = ry_sum / rcnt; }
    else          { *rx = mid_x + bw/5;   *ry = eye_top + bh/3; }

    /* Symmetrize */
    int avg_ey = (*ly + *ry) / 2;
    int eye_dist = (*rx - *lx) / 2;
    *lx = mid_x - eye_dist;
    *rx = mid_x + eye_dist;
    *ly = avg_ey;
    *ry = avg_ey;

    free(dark_x); free(dark_y);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.png/jpg output.svg [options]\n", argv[0]);
        fprintf(stderr, "  --eps N     Douglas-Peucker epsilon (default: 1.0)\n");
        fprintf(stderr, "  --invert    Invert mask (subject darker than bg)\n");
        fprintf(stderr, "  --eyes      Add eye/nose dots\n");
        fprintf(stderr, "  --crop      Auto-crop to subject bounding box\n");
        return 1;
    }

    const char *input  = argv[1];
    const char *output = argv[2];
    double eps = 1.0;
    int invert = 0, do_eyes = 0, do_crop = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--invert") == 0) invert = 1;
        else if (strcmp(argv[i], "--eyes") == 0) do_eyes = 1;
        else if (strcmp(argv[i], "--crop") == 0) do_crop = 1;
        else if (strcmp(argv[i], "--eps") == 0 && i+1 < argc)
            eps = atof(argv[++i]);
    }

    /* Load image */
    int w, h, channels;
    unsigned char *img = stbi_load(input, &w, &h, &channels, 3);
    if (!img) { fprintf(stderr, "Error: cannot load '%s'\n", input); return 1; }
    printf("Loaded %s: %dx%d, %d channels\n", input, w, h, channels);

    /* Convert to grayscale */
    uint8_t *gray = malloc((size_t)w * h);
    uint8_t *mask = malloc((size_t)w * h);
    uint8_t *tmp  = malloc((size_t)w * h);

    for (int i = 0; i < w * h; i++)
        gray[i] = rgb_luminance(img[i*3], img[i*3+1], img[i*3+2]);

    /* Otsu threshold */
    int thresh = otsu_threshold(gray, w * h);
    printf("Otsu threshold: %d\n", thresh);

    /* Flood fill mask */
    flood_fill_mask(gray, mask, w, h, thresh, invert);

    /* Clean mask */
    median_filter_3(mask, tmp, w, h); memcpy(mask, tmp, (size_t)w*h);
    morph_filter(mask, tmp, w, h, 3, 0); memcpy(mask, tmp, (size_t)w*h);
    morph_filter(mask, tmp, w, h, 3, 1); memcpy(mask, tmp, (size_t)w*h);

    /* Count subject pixels */
    int sc = 0;
    for (int i = 0; i < w * h; i++) if (mask[i] > 128) sc++;
    printf("Subject pixels: %d/%d (%.1f%%)\n", sc, w*h, 100.0*sc/(w*h));

    /* Bounding box */
    int min_x, max_x, min_y, max_y;
    mask_bbox(mask, w, h, &min_x, &max_x, &min_y, &max_y);
    int bw = max_x - min_x, bh = max_y - min_y;
    printf("BBox: (%d,%d) -> (%d,%d)  %dx%d\n", min_x, min_y, max_x, max_y, bw, bh);

    /* Trace contour */
    vec2_arr contour;
    arr_init(&contour);
    int cn = moore_neighbor_trace(mask, w, h, &contour);
    if (cn < 0) { fprintf(stderr, "No subject found in image\n"); return 1; }
    printf("Contour: %d points\n", cn);

    /* Subtract bbox offset for crop mode */
    int ox = 0, oy = 0;
    if (do_crop) { ox = min_x - 10; oy = min_y - 10; if (ox < 0) ox = 0; if (oy < 0) oy = 0; }
    int out_w = do_crop ? (max_x - min_x + 20) : w;
    int out_h = do_crop ? (max_y - min_y + 20) : h;

    /* Adjust contour coords for crop */
    vec2 *pts = contour.data;
    for (int i = 0; i < contour.len; i++) { pts[i].x -= ox; pts[i].y -= oy; }

    /* Simplify */
    int n_simple = dp_simplify(pts, contour.len, eps);
    printf("Simplified: %d points (eps=%.1f)\n", n_simple, eps);

    /* Build SVG */
    svg_buf svg;
    svg_init(&svg);

    /* Extract primary subject color */
    int r_sum = 0, g_sum = 0, b_sum = 0, c_count = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (mask[y*w + x] > 128) {
                int idx = y * w * 3 + x * 3;
                r_sum += img[idx]; g_sum += img[idx+1]; b_sum += img[idx+2];
                c_count++;
            }
        }
    }
    int pr = c_count ? r_sum / c_count : 200;
    int pg = c_count ? g_sum / c_count : 100;
    int pb = c_count ? b_sum / c_count : 50;

    /* Darken for stroke/shadow */
    int dr = pr/3, dg = pg/3, db = pb/3;

    svg_printf(&svg,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\""
        " width=\"%d\" height=\"%d\">\n",
        out_w, out_h, out_w/2, out_h/2);

    svg_printf(&svg,
        "  <defs>\n"
        "    <radialGradient id=\"fur\" cx=\"50%%\" cy=\"35%%\" r=\"65%%\">\n"
        "      <stop offset=\"0%%\" stop-color=\"#%02x%02x%02x\"/>\n"
        "      <stop offset=\"100%%\" stop-color=\"#%02x%02x%02x\"/>\n"
        "    </radialGradient>\n"
        "  </defs>\n", pr, pg, pb, dr, dg, db);

    svg_printf(&svg, "  <path d=\"");
    svg_path_from_contour(&svg, pts, n_simple, 0, 0);
    svg_printf(&svg, "\" fill=\"url(#fur)\" stroke=\"#%02x%02x%02x\" stroke-width=\"1.5\"/>\n",
               dr, dg, db);

    /* Eyes and nose */
    if (do_eyes) {
        int lx, ly, rx, ry;
        int bbox_a[4] = {min_x, max_x, min_y, max_y};
        find_eyes(img, mask, w, h, bbox_a, &lx, &ly, &rx, &ry);

        int eye_r = bw / 30; if (eye_r < 3) eye_r = 3;
        int nose_x = (lx + rx) / 2;
        int nose_y = ly + (int)(bh * 0.14);
        int nose_r = bw / 40; if (nose_r < 2) nose_r = 2;

        svg_printf(&svg,
            "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#%02x%02x%02x\"/>\n"
            "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#%02x%02x%02x\"/>\n"
            "  <ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" fill=\"#%02x%02x%02x\"/>\n",
            lx - ox, ly - oy, eye_r, dr, dg, db,
            rx - ox, ry - oy, eye_r, dr, dg, db,
            nose_x - ox, nose_y - oy, nose_r, nose_r/2, dr, dg, db);
    }

    svg_printf(&svg, "</svg>\n");

    /* Write output */
    FILE *fp = fopen(output, "wb");
    if (!fp) { fprintf(stderr, "Error: cannot write '%s'\n", output); return 1; }
    fwrite(svg.buf, 1, svg.len, fp);
    fclose(fp);
    printf("Wrote %s (%zu bytes)\n", output, svg.len);

    /* Cleanup */
    stbi_image_free(img);
    free(gray); free(mask); free(tmp);
    arr_free(&contour);
    svg_free(&svg);
    return 0;
}
