/*
 * image_gen.c — Image generation tool for Hermes C.
 * Uses FAL.ai REST API: POST to fal-ai/flux-pro with prompt.
 * Reads FAL_API_KEY from config/env.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  Image Generation via FAL.ai REST API
 * ================================================================ */

#define FAL_API_BASE "https://fal.run/fal-ai/flux-pro"

/* Generate an image from a text prompt */
char *image_generate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args) return strdup("{\"success\":false,\"error\":\"Invalid JSON arguments\"}");

    const char *prompt = json_get_str(args, "prompt", "");
    const char *aspect_ratio = json_get_str(args, "aspect_ratio", "1:1");

    /* Get API key from environment */
    const char *api_key = getenv("FAL_API_KEY");
    if (!api_key || !*api_key)
        api_key = getenv("SLERMES_FAL_KEY");
    if (!api_key || !*api_key) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"FAL_API_KEY not set. Get a key at https://fal.ai\"}");
    }

    if (!prompt || !*prompt) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'prompt' parameter\"}");
    }

    /* Escape prompt for JSON */
    char esc_prompt[4096];
    size_t ei = 0;
    for (const char *sp = prompt; *sp && ei < sizeof(esc_prompt) - 4; sp++) {
        if (*sp == '"' || *sp == '\\') { esc_prompt[ei++] = '\\'; esc_prompt[ei++] = *sp; }
        else if (*sp == '\n') { esc_prompt[ei++] = '\\'; esc_prompt[ei++] = 'n'; }
        else if (*sp == '\t') { esc_prompt[ei++] = '\\'; esc_prompt[ei++] = 't'; }
        else esc_prompt[ei++] = *sp;
    }
    esc_prompt[ei] = '\0';

    /* Build request body */
    char body[8192];
    snprintf(body, sizeof(body),
        "{\"prompt\":\"%s\",\"aspect_ratio\":\"%s\"}",
        esc_prompt, aspect_ratio);

    /* Create HTTP client and make POST request */
    http_t *h = http_new(60);
    if (!h) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to create HTTP client\"}");
    }

    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Key %s\r\n", api_key);

    http_resp_t *resp = http_post_json_auth(h, FAL_API_BASE, body, auth_hdr);

    if (!resp) {
        http_free(h);
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to connect to FAL API\"}");
    }

    if (resp->status != 200) {
        char err[1024];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"FAL API returned HTTP %d: %s\"}",
            resp->status, resp->body ? resp->body : "unknown");
        http_resp_free(resp);
        http_free(h);
        json_free(args);
        return strdup(err);
    }

    /* Parse response */
    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp);
    http_free(h);

    if (!result) {
        char err[1024];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"Failed to parse FAL response: %s\"}",
            parse_err ? parse_err : "parse error");
        free(parse_err);
        json_free(args);
        return strdup(err);
    }

    /* Extract image URL from response */
    json_t *images = json_obj_get(result, "images");
    const char *image_url = NULL;
    if (images && json_len(images) > 0) {
        json_t *first = json_get(images, 0);
        if (first) image_url = json_get_str(first, "url", NULL);
    }

    if (!image_url) {
        char *s = json_serialize(result);
        char err[2048];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"No image URL in response\",\"response\":%s}",
            s ? s : "null");
        free(s);
        json_free(result);
        json_free(args);
        return strdup(err);
    }

    /* Download the image to a local file */
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/slermes_img_%ld.png",
        (long)time(NULL));

    http_t *dh = http_new(30);
    http_resp_t *img_resp = http_get(dh, image_url, NULL);
    int dl_ok = 0;
    if (img_resp && img_resp->status == 200 && img_resp->body && img_resp->body_len > 0) {
        FILE *f = fopen(filename, "wb");
        if (f) {
            fwrite(img_resp->body, 1, img_resp->body_len, f);
            fclose(f);
            dl_ok = 1;
        }
    }
    if (img_resp) http_resp_free(img_resp);
    if (dh) http_free(dh);

    /* Build response */
    char out[8192];
    if (dl_ok) {
        snprintf(out, sizeof(out),
            "{\"success\":true,\"image\":\"%s\",\"local_path\":\"%s\"}", image_url, filename);
    } else {
        snprintf(out, sizeof(out),
            "{\"success\":true,\"image\":\"%s\",\"warning\":\"Could not download image\"}", image_url);
    }

    json_free(result);
    json_free(args);
    return strdup(out);
}

/* ================================================================
 *  Registration
 * ================================================================ */

void registry_init_image_gen(void) {
    registry_register("image_generate",
        "Generate an image from a text prompt using the configured FAL API.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
        "  \"prompt\":{\"type\":\"string\",\"description\":\"Text description of the image to generate\"},"
        "  \"aspect_ratio\":{\"type\":\"string\",\"description\":\"Aspect ratio (e.g., 1:1, 16:9, 9:16)\"}"
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        image_generate_handler);
}
