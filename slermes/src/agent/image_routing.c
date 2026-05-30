/*
 * image_routing.c — Routing helpers for inbound user-attached images.
 *
 * Port of Python agent/image_routing.py (391L). Two modes:
 *   native  — attach images as data URL content parts
 *   text    — run vision_analyze up-front, text summary only
 *
 * Decision per message turn by decide_image_input_mode().
 */

#include "image_routing.h"
#include "hermes.h"
#include "provider_metadata.h"
#include "../lib/libbase64/base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Static helpers ────────────────────────────────────────────── */

static bool is_valid_image_mode(const char *mode) {
    if (!mode || !*mode) return false;
    static const char *valid[] = {"auto", "native", "text", NULL};
    for (const char **p = valid; *p; p++) {
        if (strcmp(mode, *p) == 0) return true;
    }
    return false;
}


/* ── MIME type detection ──────────────────────────────────────── */

const char *sniff_mime_from_bytes(const unsigned char *data, size_t len) {
    if (!data || len == 0) return NULL;

    /* PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (len >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' &&
        data[3] == 'G' && data[4] == 0x0D && data[5] == 0x0A &&
        data[6] == 0x1A && data[7] == 0x0A)
        return "image/png";

    /* JPEG: FF D8 FF */
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return "image/jpeg";

    /* GIF87a / GIF89a */
    if (len >= 6) {
        if (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)
            return "image/gif";
    }

    /* WEBP: "RIFF" .... "WEBP" */
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0)
        return "image/webp";

    /* BMP: "BM" */
    if (len >= 2 && data[0] == 'B' && data[1] == 'M')
        return "image/bmp";

    /* HEIC/HEIF: ftypheic / ftypheix / ftypmif1 / ftypmsf1 etc. */
    if (len >= 12 && memcmp(data + 4, "ftyp", 4) == 0) {
        const char *brand = (const char *)data + 8;
        static const char *heic_brands[] = {
            "heic", "heix", "hevc", "hevx", "mif1", "msf1", "heim", "heis", NULL
        };
        for (const char **b = heic_brands; *b; b++) {
            if (len >= 12 && memcmp(brand, *b, 4) == 0)
                return "image/heic";
        }
    }

    return NULL;
}


const char *guess_mime(const char *path,
                       const unsigned char *data,
                       size_t data_len)
{
    if (data && data_len > 0) {
        const char *sniffed = sniff_mime_from_bytes(data, data_len);
        if (sniffed) return sniffed;
    }
    if (!path) return "image/jpeg";

    /* Get suffix */
    const char *dot = strrchr(path, '.');
    if (!dot) return "image/jpeg";
    dot++; /* skip the '.' */

    char suf[16];
    size_t i;
    for (i = 0; i < sizeof(suf) - 1 && dot[i]; i++)
        suf[i] = (char)tolower((unsigned char)dot[i]);
    suf[i] = '\0';

    if (strcmp(suf, "jpg") == 0 || strcmp(suf, "jpeg") == 0) return "image/jpeg";
    if (strcmp(suf, "png") == 0)  return "image/png";
    if (strcmp(suf, "gif") == 0)  return "image/gif";
    if (strcmp(suf, "webp") == 0) return "image/webp";
    if (strcmp(suf, "bmp") == 0)  return "image/bmp";
    if (strcmp(suf, "heic") == 0 || strcmp(suf, "heif") == 0) return "image/heic";

    return "image/jpeg";
}

/* ── File to data URL ──────────────────────────────────────────── */

char *file_to_data_url(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    if (flen <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    /* Read entire file */
    unsigned char *buf = (unsigned char *)malloc((size_t)flen);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)flen, f);
    fclose(f);
    if (nread != (size_t)flen) {
        free(buf);
        return NULL;
    }

    /* Detect MIME */
    const char *mime = guess_mime(path, buf, (size_t)flen);
    if (!mime) mime = "image/jpeg";

    /* Base64 encode */
    char *b64 = base64_encode(buf, (size_t)flen);
    free(buf);
    if (!b64) return NULL;

    /* Build data URL: data:<mime>;base64,<b64> */
    size_t url_len = strlen("data:") + strlen(mime) + strlen(";base64,") + strlen(b64) + 1;
    char *url = (char *)malloc(url_len);
    if (!url) { free(b64); return NULL; }
    snprintf(url, url_len, "data:%s;base64,%s", mime, b64);
    free(b64);
    return url;
}

/* ── Vision capability lookup ─────────────────────────────────── */

static int supports_vision_override(const hermes_config_t *cfg,
                                    const char *provider,
                                    const char *model)
{
    (void)provider;
    if (!cfg) return -1;

    /* 1. Top-level model.supports_vision shortcut */
    if (cfg->provider_cfg.supports_vision)
        return 1;

    /* 2. S06: Check per-model vision_overrides for custom model prefixes */
    if (cfg->provider_cfg.vision_overrides[0]) {
        char buf[1024];
        strncpy(buf, cfg->provider_cfg.vision_overrides, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = buf;
        while (tok && *tok) {
            while (*tok == ' ' || *tok == ',') tok++;
            if (!*tok) break;
            char *end = tok;
            while (*end && *end != ',') end++;
            int is_last = (*end == '\0');
            *end = '\0';
            char *trim = end - 1;
            while (trim >= tok && *trim == ' ') { *trim = '\0'; trim--; }
            if (*tok && model && strncasecmp(model, tok, strlen(tok)) == 0)
                { if (!is_last) *end = ','; return 1; }
            tok = is_last ? NULL : end + 1;
        }
    }

    return -1; /* unknown — fall through to model_metadata */
}

static bool explicit_aux_vision_override(const hermes_config_t *cfg) {
    if (!cfg) return false;
    const auxiliary_task_config_t *vis = &cfg->auxiliary.vision;
    /* "auto" / "" / blank = not explicit */
    if (vis->provider[0] == '\0' ||
        strcmp(vis->provider, "auto") == 0) {
        if (vis->model[0] == '\0' && vis->base_url[0] == '\0')
            return false;
    }
    return true;
}

/* Return 1=supports, 0=doesn't, -1=unknown */
static int lookup_supports_vision(const char *provider,
                                  const char *model,
                                  const hermes_config_t *cfg)
{
    int override = supports_vision_override(cfg, provider, model);
    if (override != -1) return override;

    if (!provider || !*provider || !model || !*model)
        return -1;

    /* Use model_supports_vision() from provider_metadata.c */
    bool supports = model_supports_vision(model, &cfg->provider_cfg);
    return supports ? 1 : 0;
}

/* ── Mode coercion ─────────────────────────────────────────────── */

static const char *coerce_mode(const char *raw) {
    if (!raw || !*raw) return "auto";
    /* lowercase */
    static char buf[16];
    size_t i;
    for (i = 0; i < sizeof(buf) - 1 && raw[i]; i++)
        buf[i] = (char)tolower((unsigned char)raw[i]);
    buf[i] = '\0';
    if (is_valid_image_mode(buf)) {
        static const char *mode_map[] = {"auto", "native", "text"};
        for (int j = 0; j < 3; j++) {
            if (strcmp(buf, mode_map[j]) == 0)
                return mode_map[j];
        }
    }
    return "auto";
}

/* ── Public API ────────────────────────────────────────────────── */

const char *decide_image_input_mode(const char *provider,
                                    const char *model,
                                    const void *cfg_ptr)
{
    const hermes_config_t *cfg = (const hermes_config_t *)cfg_ptr;

    /* 1. Read configured mode (auto/native/text) */
    const char *mode_cfg = "auto";
    if (cfg)
        mode_cfg = coerce_mode(cfg->agent.image_input_mode);

    if (strcmp(mode_cfg, "native") == 0)
        return "native";
    if (strcmp(mode_cfg, "text") == 0)
        return "text";

    /* auto mode */
    if (cfg && explicit_aux_vision_override(cfg))
        return "text";

    int supports = lookup_supports_vision(provider, model, cfg);
    if (supports == 1)
        return "native";

    return "text";
}

/**
 * image_routing_decide_mode: Wrapper around decide_image_input_mode that
 * also checks runtime vision-disabled state from provider errors.
 *
 * Returns "text" if vision is disabled, otherwise delegates to
 * decide_image_input_mode().
 */
const char *image_routing_decide_mode(const void *state,
                                       const char *provider,
                                       const char *model,
                                       const void *cfg)
{
    if (state) {
        /* Cast to agent_state_t — only field we access is vision_disabled */
        const agent_state_t *s = (const agent_state_t *)state;
        if (s->vision_disabled)
            return "text";
    }
    return decide_image_input_mode(provider, model, cfg);
}

char *build_native_content_parts(const char *user_text,
                                  const char **image_paths,
                                  size_t num_paths,
                                  char ***skipped_out,
                                  size_t *skipped_cnt)
{
    if (skipped_out) *skipped_out = NULL;
    if (skipped_cnt) *skipped_cnt = 0;

    /* Temporary arrays for attached / skipped */
    size_t attached_cnt = 0;
    size_t max_parts = num_paths > 0 ? num_paths : 1;
    char **data_urls = (char **)calloc(max_parts, sizeof(char *));
    char **attached_paths = (char **)calloc(max_parts, sizeof(char *));
    char **skipped = (char **)calloc(max_parts, sizeof(char *));
    size_t skip_cnt = 0;

    if (!data_urls || !attached_paths || !skipped) {
        free(data_urls); free(attached_paths); free(skipped);
        return NULL;
    }

    /* Process each image path */
    for (size_t i = 0; i < num_paths; i++) {
        const char *rp = image_paths[i];

        /* Check file exists */
        FILE *f = fopen(rp, "rb");
        if (!f) {
            skipped[skip_cnt] = strdup(rp);
            if (skipped[skip_cnt]) skip_cnt++;
            continue;
        }
        fclose(f);

        char *data_url = file_to_data_url(rp);
        if (!data_url) {
            skipped[skip_cnt] = strdup(rp);
            if (skipped[skip_cnt]) skip_cnt++;
            continue;
        }
        data_urls[attached_cnt] = data_url;
        attached_paths[attached_cnt] = strdup(rp);
        attached_cnt++;
    }

    /* Build JSON content array using the internal JSON API */
    /* We'll build it manually as a string to avoid libjson dependency for this module */
    json_t *parts = json_array();
    if (!parts) {
        for (size_t i = 0; i < attached_cnt; i++) { free(data_urls[i]); free(attached_paths[i]); }
        for (size_t i = 0; i < skip_cnt; i++) free(skipped[i]);
        free(data_urls); free(attached_paths); free(skipped);
        return NULL;
    }

    /* Text part */
    const char *base_text = user_text && user_text[0] ? user_text : "What do you see in this image?";
    char text_buf[4096];
    int text_pos = snprintf(text_buf, sizeof(text_buf), "%s", base_text);

    if (attached_cnt > 0) {
        text_pos += snprintf(text_buf + text_pos, sizeof(text_buf) - (size_t)text_pos,
                             "\n\n");
        for (size_t i = 0; i < attached_cnt; i++) {
            text_pos += snprintf(text_buf + text_pos, sizeof(text_buf) - (size_t)text_pos,
                                 "[Image attached at: %s]\n", attached_paths[i]);
        }
    }

    json_t *text_part = json_object();
    json_set(text_part, "type", json_string("text"));
    json_set(text_part, "text", json_string(text_buf));
    json_append(parts, text_part);

    /* Image parts */
    for (size_t i = 0; i < attached_cnt; i++) {
        json_t *img = json_object();
        json_set(img, "type", json_string("image_url"));
        json_t *url_obj = json_object();
        json_set(url_obj, "url", json_string(data_urls[i]));
        json_set(img, "image_url", url_obj);
        json_append(parts, img);
    }

    /* No images attached — plain text only */
    if (attached_cnt == 0) {
        json_t *plain_text = json_object();
        json_set(plain_text, "type", json_string("text"));
        json_set(plain_text, "text", json_string(text_buf));
        json_append(parts, plain_text);
    }

    /* Serialize */
    char *result = json_serialize(parts);
    json_free(parts);

    /* Cleanup data URLs and attached paths */
    for (size_t i = 0; i < attached_cnt; i++) {
        free(data_urls[i]);
        free(attached_paths[i]);
    }
    free(data_urls);
    free(attached_paths);

    /* Set output skipped paths */
    if (skipped_out && skip_cnt > 0) {
        *skipped_out = (char **)malloc(skip_cnt * sizeof(char *));
        if (*skipped_out) {
            for (size_t i = 0; i < skip_cnt; i++)
                (*skipped_out)[i] = skipped[i];
        }
    }
    if (skipped_cnt) *skipped_cnt = skip_cnt;
    for (size_t i = 0; i < skip_cnt; i++) free(skipped[i]);
    free(skipped);

    return result;
}

/* ── Vision disable (L03) ────────────────────────────────── */

void image_routing_disable_vision(void *state) {
    if (!state) return;
    agent_state_t *s = (agent_state_t *)state;
    s->vision_disabled = true;
}

bool image_routing_vision_disabled(const void *state) {
    if (!state) return false;
    const agent_state_t *s = (const agent_state_t *)state;
    return s->vision_disabled;
}

/**
 * image_routing_notify_error: Check if error suggests vision not supported.
 *
 * Triggers on patterns like:
 *   - "text only" / "text-only" model
 *   - "image" not supported / rejected
 *   - "vision" not available
 *
 * Only disables if the model/provider was thought to support vision.
 * Returns true if vision was newly disabled.
 */
bool image_routing_notify_error(void *state, const char *error_text) {
    if (!state || !error_text) return false;
    agent_state_t *s = (agent_state_t *)state;
    if (s->vision_disabled) return false; /* already disabled */

    /* Check for vision-incompatible error patterns */
    const char *patterns[] = {
        "text only",
        "text-only",
        "does not support image",
        "image input is not supported",
        "does not support vision",
        "vision is not supported",
        "content has invalid image",
        "images are not supported",
        "image_url is not supported",
        "image_url is not enabled",
        "image data is not supported",
        NULL
    };

    for (int i = 0; patterns[i]; i++) {
        if (strstr(error_text, patterns[i])) {
            s->vision_disabled = true;
            fprintf(stderr, "[image_routing] Vision auto-disabled: pattern '%s' matched in error\n",
                    patterns[i]);
            return true;
        }
    }

    return false;
}
