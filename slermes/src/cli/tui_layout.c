/*
 * tui_layout.c — App layout + chrome engine for the TUI.
 *
 * Provides configurable pane layout management with chrome decorations,
 * responsive breakpoints, and pane constraints. Replaces hardcoded
 * inline layout math in tui_fullscreen.c (~80 lines).
 *
 * MIT License — WuBu Hermes Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tui_layout.h"

/* ── Mode names ── */
static const char *s_mode_names[] = {
    "normal", "mobile", "compact", "wide",
};

const char *tui_layout_mode_name(tui_layout_mode_t mode) {
    if ((unsigned)mode < sizeof(s_mode_names) / sizeof(s_mode_names[0]))
        return s_mode_names[mode];
    return "unknown";
}

/* ── API: Init ── */
void tui_layout_init(tui_layout_t *l, int term_rows, int term_cols) {
    if (!l) return;
    memset(l, 0, sizeof(*l));
    l->term_rows = term_rows > 0 ? term_rows : 24;
    l->term_cols = term_cols > 0 ? term_cols : 80;
    l->mode = TUI_LAYOUT_NORMAL;
    l->pane_count = 0;
}

/* ── API: Set mode ── */
void tui_layout_set_mode(tui_layout_t *l, tui_layout_mode_t mode) {
    if (!l) return;
    l->mode = mode;
}

/* ── API: Add pane ── */
int tui_layout_add_pane(tui_layout_t *l,
                         const char *name,
                         tui_pane_side_t side,
                         tui_pane_size_t sizing,
                         double ratio,
                         int fixed_size,
                         int min_size,
                         int max_size,
                         tui_chrome_flags_t chrome,
                         const char *title) {
    if (!l || l->pane_count >= TUI_LAYOUT_MAX_PANES) return -1;

    int idx = l->pane_count;
    tui_layout_pane_t *p = &l->panes[idx];

    memset(p, 0, sizeof(*p));

    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
    } else {
        snprintf(p->name, sizeof(p->name), "pane_%d", idx);
    }

    p->side       = side;
    p->sizing     = sizing;
    p->ratio      = ratio;
    p->fixed_size = fixed_size;
    p->min_size   = min_size;
    p->max_size   = max_size;
    p->chrome     = chrome;

    if (title) {
        strncpy(p->title, title, sizeof(p->title) - 1);
    }

    p->y = p->x = p->rows = p->cols = 0;
    p->visible = false;

    l->pane_count++;
    return idx;
}

/* ── API: Set chrome ── */
void tui_layout_set_chrome(tui_layout_t *l, int pane_idx, tui_chrome_flags_t chrome) {
    if (!l || pane_idx < 0 || pane_idx >= l->pane_count) return;
    l->panes[pane_idx].chrome = chrome;
}

/* ── API: Set title ── */
void tui_layout_set_title(tui_layout_t *l, int pane_idx, const char *title) {
    if (!l || pane_idx < 0 || pane_idx >= l->pane_count || !title) return;
    strncpy(l->panes[pane_idx].title, title, sizeof(l->panes[pane_idx].title) - 1);
}

/* ── API: Find pane ── */
int tui_layout_find_pane(const tui_layout_t *l, const char *name) {
    if (!l || !name) return -1;
    for (int i = 0; i < l->pane_count; i++) {
        if (strcmp(l->panes[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* ── API: Pane visible ── */
bool tui_layout_pane_visible(const tui_layout_t *l, int pane_idx) {
    if (!l || pane_idx < 0 || pane_idx >= l->pane_count) return false;
    return l->panes[pane_idx].visible;
}

/* ── Internal: chrome size calculation ── */
int tui_layout_chrome_rows(const tui_layout_t *l, int pane_idx) {
    (void)l;
    if (pane_idx < 0) return 0;
    return 0;
}

int tui_layout_chrome_cols(const tui_layout_t *l, int pane_idx) {
    (void)l;
    if (pane_idx < 0) return 0;
    return 0;
}

/* ── API: Calculate layout ── */
void tui_layout_calculate(tui_layout_t *l) {
    if (!l) return;

    int avail_rows = l->term_rows;
    int avail_cols = l->term_cols;

    /* Reset all panes */
    for (int i = 0; i < l->pane_count; i++) {
        l->panes[i].y = l->panes[i].x = 0;
        l->panes[i].rows = l->panes[i].cols = 0;
        l->panes[i].visible = false;
    }

    /* Phase 1: Process TOP/BOTTOM panes (vertical slices) */
    int top_y = 0;
    int bottom_y = avail_rows;

    for (int pass = 0; pass < 2; pass++) {
        tui_pane_side_t target_side = (pass == 0) ? TUI_PANE_TOP : TUI_PANE_BOTTOM;

        for (int i = 0; i < l->pane_count; i++) {
            tui_layout_pane_t *p = &l->panes[i];
            if (p->side != target_side) continue;

            int h = 0;

            switch (p->sizing) {
            case TUI_PANE_FIXED:
                h = p->fixed_size;
                break;
            case TUI_PANE_RATIO:
                h = (int)(avail_rows * p->ratio);
                break;
            case TUI_PANE_AUTO:
                h = 0; /* No data yet */
                break;
            case TUI_PANE_FILL:
                /* FILL panes in TOP/BOTTOM are rare — treat as RATIO 0.2 */
                h = avail_rows / 5;
                break;
            }

            /* Apply constraints */
            if (p->min_size > 0 && h < p->min_size) h = p->min_size;
            if (p->max_size > 0 && h > p->max_size) h = p->max_size;
            /* Only clamp to 1 if the pane has a positive size */
            if (h > 0 && h < 1) h = 1;

            if (target_side == TUI_PANE_TOP) {
                p->y = top_y;
                p->x = 0;
                p->rows = h;
                p->cols = avail_cols;
                top_y += h;
            } else {
                bottom_y -= h;
                p->y = bottom_y;
                p->x = 0;
                p->rows = h;
                p->cols = avail_cols;
            }

            p->visible = (h > 0);
        }
    }

    /* Phase 2: Remaining vertical space for center / left / right panes */
    int center_y = top_y;
    int center_h = bottom_y - top_y;
    if (center_h < 0) center_h = 0;

    /* Phase 3: Process LEFT/RIGHT panes within center area */
    int left_x = 0;
    int right_x = avail_cols;

    for (int pass = 0; pass < 2; pass++) {
        tui_pane_side_t target_side = (pass == 0) ? TUI_PANE_LEFT : TUI_PANE_RIGHT;

        for (int i = 0; i < l->pane_count; i++) {
            tui_layout_pane_t *p = &l->panes[i];
            if (p->side != target_side) continue;

            int w = 0;

            switch (p->sizing) {
            case TUI_PANE_FIXED:
                w = p->fixed_size;
                break;
            case TUI_PANE_RATIO:
                w = (int)(avail_cols * p->ratio);
                break;
            case TUI_PANE_AUTO:
                w = 20; /* Default auto-width */
                break;
            case TUI_PANE_FILL:
                w = avail_cols / 4;
                break;
            }

            /* Apply constraints */
            if (p->min_size > 0 && w < p->min_size) w = p->min_size;
            if (p->max_size > 0 && w > p->max_size) w = p->max_size;
            if (w > 0 && w < 1) w = 1;
            /* Don't let side panes take more than 50% */
            if (w > avail_cols / 2) w = avail_cols / 2;

            if (target_side == TUI_PANE_LEFT) {
                p->x = left_x;
                p->y = center_y;
                p->cols = w;
                p->rows = center_h;
                left_x += w;
            } else {
                right_x -= w;
                p->x = right_x;
                p->y = center_y;
                p->cols = w;
                p->rows = center_h;
            }

            p->visible = (w > 0 && center_h > 1);
        }
    }

    /* Phase 4: CENTER panes fill remaining space */
    int center_w = right_x - left_x;
    if (center_w < 0) center_w = 0;

    for (int i = 0; i < l->pane_count; i++) {
        tui_layout_pane_t *p = &l->panes[i];
        if (p->side != TUI_PANE_CENTER) continue;

        p->y = center_y;
        p->x = left_x;
        p->rows = center_h;
        p->cols = center_w;
        p->visible = (center_h > 0 && center_w > 0);
    }
}

/* ── API: Resize ── */
void tui_layout_resize(tui_layout_t *l, int term_rows, int term_cols) {
    if (!l) return;
    l->term_rows = term_rows > 0 ? term_rows : 24;
    l->term_cols = term_cols > 0 ? term_cols : 80;
    tui_layout_calculate(l);
}

/* ── API: Get pane ── */
const tui_layout_pane_t *tui_layout_get_pane(const tui_layout_t *l, int pane_idx) {
    if (!l || pane_idx < 0 || pane_idx >= l->pane_count) return NULL;
    return &l->panes[pane_idx];
}

/* ── API: Pane count ── */
int tui_layout_pane_count(const tui_layout_t *l) {
    return l ? l->pane_count : 0;
}
