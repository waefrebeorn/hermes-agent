/*
 * image_gen.c — Image generation tool for Hermes C.
 * Uses FAL.ai REST API: POST to fal-ai/flux-pro with prompt.
 * Reads FAL_API_KEY from config/env via libfalcommon.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "fal_common.h"
#include "image_gen_registry.h"
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
    const char *negative_prompt = json_get_str(args, "negative_prompt", NULL);
    const char *style = json_get_str(args, "style", NULL);
    const char *image_url = json_get_str(args, "image_url", NULL);
    int seed = (int)json_get_num(args, "seed", 0);
    int num_images = (int)json_get_num(args, "num_images", 1);

    /* Get API key from shared helper (checks FAL_API_KEY, then SLERMES_FAL_KEY) */
    if (!fal_get_api_key()) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"FAL_API_KEY not set. Get a key at https://fal.ai\"}");
    }

    if (!prompt || !*prompt) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Missing 'prompt' parameter\"}");
    }

    /* Escape prompt for JSON */
    char esc_prompt[4096];
    fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));

    /* Build request body with optional params */
    char body[16384];
    int pos = snprintf(body, sizeof(body),
        "{\"prompt\":\"%s\",\"aspect_ratio\":\"%s\"",
        esc_prompt, aspect_ratio);

    if (image_url && *image_url) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"image_url\":\"%s\"", image_url);
    }
    if (negative_prompt && *negative_prompt) {
        char esc_neg[2048];
        fal_escape_json(negative_prompt, esc_neg, sizeof(esc_neg));
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"negative_prompt\":\"%s\"", esc_neg);
    }
    if (style && *style) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"style\":\"%s\"", style);
    }
    if (seed > 0) {
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"seed\":%d", seed);
    }
    if (num_images > 1) {
        if (num_images > 4) num_images = 4;
        pos += snprintf(body + pos, sizeof(body) - pos,
            ",\"num_images\":%d", num_images);
    }
    snprintf(body + pos, sizeof(body) - pos, "}");

    /* Use shared FAL POST helper */
    http_resp_t *resp = fal_post_json(FAL_API_BASE, body, 60);

    if (!resp) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to connect to FAL API\"}");
    }

    if (resp->status != 200) {
        char *err = fal_error_from_http(resp);
        http_resp_free(resp);
        json_free(args);
        return err;
    }

    /* Parse response */
    char *parse_err = NULL;
    json_t *result = json_parse(resp->body, &parse_err);
    http_resp_free(resp);

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
    const char *result_image_url = NULL;
    if (images && json_len(images) > 0) {
        json_t *first = json_get(images, 0);
        if (first) result_image_url = json_get_str(first, "url", NULL);
    }

    if (!result_image_url) {
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
    http_resp_t *img_resp = http_get(dh, result_image_url, NULL);
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
            "{\"success\":true,\"image\":\"%s\",\"local_path\":\"%s\"}", result_image_url, filename);
    } else {
        snprintf(out, sizeof(out),
            "{\"success\":true,\"image\":\"%s\",\"warning\":\"Could not download image\"}", result_image_url);
    }

    json_free(result);
    json_free(args);
    return strdup(out);
}

/* ================================================================
 *  Registration
 * ================================================================ */

/* FAL provider availability check */
static bool fal_image_is_available(void) {
    return fal_get_api_key() != NULL;
}

void registry_init_image_gen(void) {
    /* Register FAL provider in the image_gen registry */
    image_gen_provider_t fal_provider;
    memset(&fal_provider, 0, sizeof(fal_provider));
    snprintf(fal_provider.name, sizeof(fal_provider.name), "%s", "fal");
    snprintf(fal_provider.display_name, sizeof(fal_provider.display_name), "%s", "FAL.ai (Flux-Pro)");
    fal_provider.is_available = fal_image_is_available;
    image_gen_register_provider(&fal_provider);

    registry_register("image_generate",
        "Generate an image from a text prompt using the configured FAL API.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
        "  \"prompt\":{\"type\":\"string\",\"description\":\"Text description of the image to generate\"},"
        "  \"aspect_ratio\":{\"type\":\"string\",\"description\":\"Aspect ratio (e.g., 1:1, 16:9, 9:16)\"},"
        "  \"negative_prompt\":{\"type\":\"string\",\"description\":\"What to avoid in the generated image\"},"
        "  \"style\":{\"type\":\"string\",\"description\":\"Style preset (e.g., realistic, anime, cinematic, digital-art, fantasy)\"},"
        "  \"seed\":{\"type\":\"integer\",\"description\":\"Random seed for reproducibility (0=random)\"},\""
        "  \"num_images\":{\"type\":\"integer\",\"description\":\"Number of images to generate (1-4)\",\"default\":1},\""
        "  \"image_url\":{\"type\":\"string\",\"description\":\"Reference image URL for image-to-image generation (img2img). Provide a URL to an existing image as source.\"}\""
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        image_generate_handler);
}
