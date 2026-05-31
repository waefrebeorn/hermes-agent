/* extras/common.h — Shared image processing utilities
 * All functions are pure C99, portable, zero-dependency beyond libc.
 *
 * Color handling: RGB triple stored as uint8_t[3], luminance via Rec.601.
 * Point/Vec2 for contour tracing.
 * SVG path building helpers.
 */

#ifndef EXTRAS_COMMON_H
#define EXTRAS_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Color helpers ── */

static inline uint8_t rgb_luminance(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((r * 299u + g * 587u + b * 114u) / 1000u);
}

static inline int rgb_warmth(uint8_t r, uint8_t g, uint8_t b) {
    return (int)r + (int)g / 2 - (int)b / 2;
}

/* ── Vec2 for contour points ── */

typedef struct { int x, y; } vec2;

static inline int vec2_dist2(vec2 a, vec2 b) {
    int dx = a.x - b.x, dy = a.y - b.y;
    return dx*dx + dy*dy;
}

static inline double vec2_perp_dist(vec2 p, vec2 s, vec2 e) {
    int dx = e.x - s.x, dy = e.y - s.y;
    if (dx == 0 && dy == 0)
        return sqrt((double)((p.x-s.x)*(p.x-s.x) + (p.y-s.y)*(p.y-s.y)));
    double t = ((double)((p.x-s.x)*dx + (p.y-s.y)*dy)) / (double)(dx*dx + dy*dy);
    if (t < 0.0) { t = 0.0; }
    if (t > 1.0) { t = 1.0; }
    double nx = s.x + t*dx, ny = s.y + t*dy;
    return sqrt((p.x-nx)*(p.x-nx) + (p.y-ny)*(p.y-ny));
}

/* ── Dynamic array for contour points ── */

typedef struct {
    vec2 *data;
    int len, cap;
} vec2_arr;

static void arr_init(vec2_arr *a) {
    a->data = NULL; a->len = 0; a->cap = 0;
}

static void arr_push(vec2_arr *a, vec2 v) {
    if (a->len >= a->cap) {
        a->cap = a->cap ? a->cap * 2 : 1024;
        a->data = realloc(a->data, (size_t)a->cap * sizeof(vec2));
    }
    a->data[a->len++] = v;
}

static void arr_free(vec2_arr *a) { free(a->data); a->data = NULL; a->len = a->cap = 0; }

/* ── SVG path writer ── */

typedef struct {
    char *buf;
    size_t len, cap;
} svg_buf;

static void svg_init(svg_buf *s) {
    s->cap = 8192; s->len = 0;
    s->buf = malloc(s->cap);
    s->buf[0] = '\0';
}

static void svg_printf(svg_buf *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
    va_end(ap);
    if (needed < 0) return;
    size_t needed_sz = (size_t)needed + 1;
    if (s->len + needed_sz > s->cap) {
        while (s->len + needed_sz > s->cap) s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
        va_start(ap, fmt);
        vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
        va_end(ap);
    }
    s->len += (size_t)needed;
}

static void svg_path_from_contour(svg_buf *s, vec2 *pts, int n, int offset_x, int offset_y) {
    for (int i = 0; i < n; i++) {
        if (i == 0) svg_printf(s, "M%d,%d", pts[i].x + offset_x, pts[i].y + offset_y);
        else        svg_printf(s, "L%d,%d", pts[i].x + offset_x, pts[i].y + offset_y);
    }
    svg_printf(s, "Z");
}

static void svg_free(svg_buf *s) { free(s->buf); s->buf = NULL; s->len = s->cap = 0; }

/* ── Douglas-Peucker ── */

static int dp_simplify_rec(vec2 *pts, int start, int end, double eps, vec2 *out, int out_idx) {
    if (start >= end) return out_idx;
    int best_i = 0;
    double best_d = 0.0;
    for (int i = start + 1; i < end; i++) {
        double d = vec2_perp_dist(pts[i], pts[start], pts[end]);
        if (d > best_d) { best_d = d; best_i = i; }
    }
    if (best_d > eps) {
        out_idx = dp_simplify_rec(pts, start, best_i, eps, out, out_idx);
        out_idx = dp_simplify_rec(pts, best_i, end, eps, out, out_idx);
        return out_idx;
    }
    out[out_idx++] = pts[end];
    return out_idx;
}

static int dp_simplify(vec2 *pts, int n, double eps) {
    /* In-place simplification — overwrites pts with simplified list */
    vec2 *tmp = malloc((size_t)n * sizeof(vec2));
    tmp[0] = pts[0];
    int cnt = dp_simplify_rec(pts, 0, n - 1, eps, tmp, 1);
    memcpy(pts, tmp, (size_t)cnt * sizeof(vec2));
    free(tmp);
    return cnt;
}

/* ── Moore-Neighbor contour tracing ── */

static int moore_neighbor_trace(uint8_t *mask, int w, int h, vec2_arr *out) {
    /* Find first white pixel */
    int sx = -1, sy = -1;
    for (int y = 0; y < h && sx < 0; y++)
        for (int x = 0; x < w && sx < 0; x++)
            if (mask[y * w + x] > 128) { sx = x; sy = y; }
    if (sx < 0) return -1;

    static const int8_t dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    static const int8_t dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

    int cx = sx, cy = sy, dir = 6; /* start searching from direction 6 */
    int limit = w * h * 2;
    int count = 0;

    do {
        arr_push(out, (vec2){cx, cy}); count++;
        if (count > limit) break;
        int found = 0;
        for (int i = 0; i < 8; i++) {
            int nd = (dir + 1 + i) % 8;
            int nx = cx + dx[nd], ny = cy + dy[nd];
            if (nx >= 0 && nx < w && ny >= 0 && ny < h && mask[ny * w + nx] > 128) {
                cx = nx; cy = ny; dir = (nd + 4) % 8;
                found = 1; break;
            }
        }
        if (!found) break;
    } while (!(cx == sx && cy == sy));

    return count;
}

#endif /* EXTRAS_COMMON_H */
