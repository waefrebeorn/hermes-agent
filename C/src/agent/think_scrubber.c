/*
 * think_scrubber.c — Streaming think/reasoning block scrubber.
 *
 * Stateful scrubber that strips <think>, <thinking>, <reasoning>,
 * <thought>, and <REASONING_SCRATCHPAD> blocks from streamed assistant
 * text. Handles partial tags split across streaming deltas.
 *
 * Tag variants handled (case-insensitive):
 *   <think>, <thinking>, <reasoning>, <thought>, <REASONING_SCRATCHPAD>
 *
 * Rules:
 *   - Closed pairs (<tag>...</tag>) are ALWAYS stripped.
 *   - Unterminated open tags stripped only at "block boundaries".
 *   - Orphan close tags (</tag> without matching open) always stripped.
 *   - Partial tags at delta boundaries held back until resolved.
 *
 * Ported from Python hermes-agent/agent/think_scrubber.py.
 */

#include "hermes_think_scrubber.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Tag definitions */
#define MAX_TAG_VARIANTS 5
#define MAX_TAG_LEN      24

static const char *OPEN_TAGS[MAX_TAG_VARIANTS] = {
    "<think>", "<thinking>", "<reasoning>", "<thought>", "<REASONING_SCRATCHPAD>"
};
static const char *CLOSE_TAGS[MAX_TAG_VARIANTS] = {
    "</think>", "</thinking>", "</reasoning>", "</thought>", "</REASONING_SCRATCHPAD>"
};
static const int TAG_COUNT = MAX_TAG_VARIANTS;

/* Pre-computed lowercase + lengths */
static char OPEN_TAGS_LOWER[MAX_TAG_VARIANTS][MAX_TAG_LEN];
static char CLOSE_TAGS_LOWER[MAX_TAG_VARIANTS][MAX_TAG_LEN];
static int TAG_OPEN_LENS[MAX_TAG_VARIANTS];
static int TAG_CLOSE_LENS[MAX_TAG_VARIANTS];
static int MAX_TAG_CHAR_LEN = 0;
static int g_tags_init = 0;

static void init_tags(void) {
    if (g_tags_init) return;
    for (int i = 0; i < TAG_COUNT; i++) {
        int olen = (int)strlen(OPEN_TAGS[i]);
        int clen = (int)strlen(CLOSE_TAGS[i]);
        TAG_OPEN_LENS[i] = olen;
        TAG_CLOSE_LENS[i] = clen;
        for (int j = 0; j < olen && j < MAX_TAG_LEN - 1; j++)
            OPEN_TAGS_LOWER[i][j] = (char)tolower((unsigned char)OPEN_TAGS[i][j]);
        OPEN_TAGS_LOWER[i][olen] = '\0';
        for (int j = 0; j < clen && j < MAX_TAG_LEN - 1; j++)
            CLOSE_TAGS_LOWER[i][j] = (char)tolower((unsigned char)CLOSE_TAGS[i][j]);
        CLOSE_TAGS_LOWER[i][clen] = '\0';
        if (olen > MAX_TAG_CHAR_LEN) MAX_TAG_CHAR_LEN = olen;
        if (clen > MAX_TAG_CHAR_LEN) MAX_TAG_CHAR_LEN = clen;
    }
    g_tags_init = 1;
}

/* Case-insensitive position of tag starting at or after offset */
static int find_tag_lower(const char *text, int text_len, int offset,
                          const char *tag_lower, int tag_len) {
    if (offset + tag_len > text_len) return -1;
    for (int i = offset; i <= text_len - tag_len; i++) {
        int match = 1;
        for (int j = 0; j < tag_len; j++)
            if (tolower((unsigned char)text[i + j]) != (unsigned char)tag_lower[j])
                { match = 0; break; }
        if (match) return i;
    }
    return -1;
}

/* Find earliest closed <tag>...</tag> pair. Returns positions. */
static void find_closed_pair(const char *buf, int buf_len,
                              int *out_start, int *out_end) {
    *out_start = -1; *out_end = -1;
    for (int i = 0; i < TAG_COUNT; i++) {
        int open = find_tag_lower(buf, buf_len, 0, OPEN_TAGS_LOWER[i], TAG_OPEN_LENS[i]);
        if (open == -1) continue;
        int close = find_tag_lower(buf, buf_len, open + TAG_OPEN_LENS[i],
                                    CLOSE_TAGS_LOWER[i], TAG_CLOSE_LENS[i]);
        if (close == -1) continue;
        int end = close + TAG_CLOSE_LENS[i];
        if (*out_start == -1 || open < *out_start) { *out_start = open; *out_end = end; }
    }
}

/* True if position idx is a block boundary */
static int is_boundary(const char *buf, int idx, int last_nl) {
    if (idx == 0) return last_nl;
    int found_nl = -1;
    for (int i = idx - 1; i >= 0; i--) { if (buf[i] == '\n') { found_nl = i; break; } }
    if (found_nl == -1) {
        if (!last_nl) return 0;
        for (int i = 0; i < idx; i++)
            if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n' && buf[i] != '\r') return 0;
        return 1;
    }
    for (int i = found_nl + 1; i < idx; i++)
        if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r') return 0;
    return 1;
}

/* Longest suffix matching a tag prefix */
static int max_partial_suffix(const char *text, int text_len) {
    if (text_len <= 0) return 0;
    int max_check = text_len < MAX_TAG_CHAR_LEN - 1 ? text_len : MAX_TAG_CHAR_LEN - 1;
    for (int i = max_check; i > 0; i--) {
        for (int t = 0; t < TAG_COUNT; t++) {
            if (TAG_CLOSE_LENS[t] > i) {
                int m = 1;
                for (int j = 0; j < i; j++)
                    if (tolower((unsigned char)text[text_len - i + j]) !=
                        (unsigned char)CLOSE_TAGS_LOWER[t][j]) { m = 0; break; }
                if (m) return i;
            }
            if (TAG_OPEN_LENS[t] > i) {
                int m = 1;
                for (int j = 0; j < i; j++)
                    if (tolower((unsigned char)text[text_len - i + j]) !=
                        (unsigned char)OPEN_TAGS_LOWER[t][j]) { m = 0; break; }
                if (m) return i;
            }
        }
    }
    return 0;
}

/* Strip orphan close tags, write to out. Returns written bytes. */
static int strip_orphan(const char *text, int text_len, char *out, int cap) {
    int opos = 0;
    for (int i = 0; i < text_len; ) {
        if (i + 1 < text_len && text[i] == '<' && text[i+1] == '/') {
            int hit = 0;
            for (int t = 0; t < TAG_COUNT; t++) {
                int tl = TAG_CLOSE_LENS[t];
                if (i + tl > text_len) continue;
                int m = 1;
                for (int j = 0; j < tl; j++)
                    if (tolower((unsigned char)text[i+j]) !=
                        (unsigned char)CLOSE_TAGS_LOWER[t][j]) { m = 0; break; }
                if (m) {
                    i += tl;
                    while (i < text_len && (text[i]==' '||text[i]=='\t'||text[i]=='\n'||text[i]=='\r')) i++;
                    hit = 1; break;
                }
            }
            if (hit) continue;
        }
        if (opos < cap - 1) out[opos++] = text[i];
        i++;
    }
    out[opos] = '\0';
    return opos;
}

/* Scrubber state */
struct think_scrubber {
    char *buf;
    int   buf_len;
    int   buf_cap;
    int   in_block;
    int   last_nl;
    char *out;
    int   out_cap;
};

/* Public API */

think_scrubber_t *think_scrubber_new(void) {
    init_tags();
    think_scrubber_t *sc = (think_scrubber_t *)calloc(1, sizeof(think_scrubber_t));
    if (!sc) return NULL;
    sc->buf_cap = 4096; sc->buf = (char *)malloc((size_t)sc->buf_cap);
    sc->out_cap = 65536; sc->out = (char *)malloc((size_t)sc->out_cap);
    sc->last_nl = 1;
    if (!sc->buf || !sc->out) { free(sc->buf); free(sc->out); free(sc); return NULL; }
    sc->buf[0] = '\0';
    return sc;
}

void think_scrubber_free(think_scrubber_t *sc) {
    if (!sc) return;
    free(sc->buf); free(sc->out); free(sc);
}

void think_scrubber_reset(think_scrubber_t *sc) {
    if (!sc) return;
    sc->buf_len = 0; sc->buf[0] = '\0'; sc->in_block = 0; sc->last_nl = 1;
}

static int grow_buf(think_scrubber_t *sc, int needed) {
    if (needed + 1 <= sc->out_cap) return 1;
    int nc = sc->out_cap;
    while (nc < needed + 1) nc *= 2;
    char *nb = (char *)realloc(sc->out, (size_t)nc);
    if (!nb) return 0;
    sc->out = nb; sc->out_cap = nc;
    return 1;
}

static int grow_work(think_scrubber_t *sc, int needed) {
    if (needed + 1 <= sc->buf_cap) return 1;
    int nc = sc->buf_cap;
    while (nc < needed + 1) nc *= 2;
    char *nb = (char *)realloc(sc->buf, (size_t)nc);
    if (!nb) return 0;
    sc->buf = nb; sc->buf_cap = nc;
    return 1;
}

const char *think_scrubber_feed(think_scrubber_t *sc, const char *text) {
    if (!sc || !text || !*text) return NULL;
    int tlen = (int)strlen(text);
    int comb = sc->buf_len + tlen;
    if (!grow_work(sc, comb)) return NULL;
    memcpy(sc->buf + sc->buf_len, text, (size_t)tlen);
    sc->buf_len = comb; sc->buf[comb] = '\0';
    if (!grow_buf(sc, comb + 1)) return NULL;
    sc->out[0] = '\0';
    int opos = 0;

    char *w = sc->buf;
    int wlen = sc->buf_len;
    int pos = 0;

    while (pos < wlen) {
        int rem = wlen - pos;
        if (sc->in_block) {
            int cp = -1, ci = -1;
            for (int i = 0; i < TAG_COUNT; i++) {
                int p = find_tag_lower(w, wlen, pos, CLOSE_TAGS_LOWER[i], TAG_CLOSE_LENS[i]);
                if (p != -1 && (cp == -1 || p < cp)) { cp = p; ci = i; }
            }
            if (cp == -1) {
                int held = max_partial_suffix(w + pos, rem);
                sc->buf_len = held;
                if (held > 0) memmove(sc->buf, w + pos + rem - held, (size_t)held);
                sc->buf[sc->buf_len] = '\0';
                sc->out[opos] = '\0';
                return opos > 0 ? sc->out : NULL;
            }
            pos = cp + TAG_CLOSE_LENS[ci];
            sc->in_block = 0;
        } else {
            int ps = -1, pe = -1;
            find_closed_pair(w + pos, rem, &ps, &pe);
            if (ps != -1) { ps += pos; pe += pos; }
            int op = -1, oi = -1;
            for (int i = 0; i < TAG_COUNT; i++) {
                int p = find_tag_lower(w, wlen, pos, OPEN_TAGS_LOWER[i], TAG_OPEN_LENS[i]);
                if (p != -1 && is_boundary(w, p, sc->last_nl) && (op == -1 || p < op)) {
                    op = p; oi = i;
                }
            }
            if (ps != -1 && (op == -1 || ps <= op)) {
                int pre = ps - pos;
                if (pre > 0) {
                    char *tmp = (char *)malloc((size_t)(pre + 1));
                    if (tmp) {
                        int st = strip_orphan(w + pos, pre, tmp, pre + 1);
                        if (opos + st + 1 > sc->out_cap) { free(tmp); return NULL; }
                        memcpy(sc->out + opos, tmp, (size_t)st); opos += st;
                        sc->last_nl = st > 0 && tmp[st-1] == '\n';
                        free(tmp);
                    }
                }
                pos = pe;
            } else if (op != -1) {
                int pre = op - pos;
                if (pre > 0) {
                    char *tmp = (char *)malloc((size_t)(pre + 1));
                    if (tmp) {
                        int st = strip_orphan(w + pos, pre, tmp, pre + 1);
                        if (opos + st + 1 > sc->out_cap) { free(tmp); return NULL; }
                        memcpy(sc->out + opos, tmp, (size_t)st); opos += st;
                        sc->last_nl = st > 0 && tmp[st-1] == '\n';
                        free(tmp);
                    }
                }
                sc->in_block = 1;
                pos = op + TAG_OPEN_LENS[oi];
            } else {
                int held = max_partial_suffix(w + pos, rem);
                int emit = rem - held;
                if (emit > 0) {
                    char *tmp = (char *)malloc((size_t)(emit + 1));
                    if (tmp) {
                        int st = strip_orphan(w + pos, emit, tmp, emit + 1);
                        if (opos + st + 1 > sc->out_cap) { free(tmp); return NULL; }
                        memcpy(sc->out + opos, tmp, (size_t)st); opos += st;
                        sc->last_nl = st > 0 && tmp[st-1] == '\n';
                        free(tmp);
                    }
                }
                sc->buf_len = held;
                if (held > 0) memmove(sc->buf, w + pos + emit, (size_t)held);
                sc->buf[sc->buf_len] = '\0';
                sc->out[opos] = '\0';
                return opos > 0 ? sc->out : NULL;
            }
        }
    }
    sc->buf_len = 0; sc->buf[0] = '\0'; sc->out[opos] = '\0';
    return opos > 0 ? sc->out : NULL;
}

const char *think_scrubber_flush(think_scrubber_t *sc) {
    if (!sc) return NULL;
    if (sc->in_block) { sc->buf_len = 0; sc->buf[0] = '\0'; sc->in_block = 0; return NULL; }
    if (sc->buf_len == 0) return NULL;
    if (!grow_buf(sc, sc->buf_len + 1)) return NULL;
    int st = strip_orphan(sc->buf, sc->buf_len, sc->out, sc->out_cap);
    sc->buf_len = 0; sc->buf[0] = '\0';
    if (st > 0) { sc->last_nl = sc->out[st-1] == '\n'; return sc->out; }
    return NULL;
}
