/*
 * video_gen.c — Video generation tool for Hermes C.
 * Uses FAL.ai REST API for text-to-video, image-to-video, and video editing.
 * Reads FAL_API_KEY from config/env via libfalcommon (shared with image_gen).
 *
 * Mirrors Python tools/video_generation_tool.py with a single unified
 * tool that dispatches to FAL.ai video endpoints.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "fal_common.h"
#include "video_gen_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  FAL.ai Video API Constants
 * ================================================================ */

/* Primary video generation endpoint — text-to-video / image-to-video */
#define FAL_VIDEO_BASE   "https://fal.run/fal-ai/veo3"

/* Video extension (continue from existing video) */
#define FAL_VIDEO_EXTEND "https://fal.run/fal-ai/video-extend"

/* ================================================================
 *  Video Generation Handler
 * ================================================================ */

char *video_generate_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json, NULL);
    if (!args)
        return strdup("{\"success\":false,\"error\":\"Invalid JSON arguments\"}");

    /* Get API key from shared helper */
    if (!fal_get_api_key()) {
        json_free(args);
        return fal_error_response("%s", "FAL_API_KEY not set. Get a key at https://fal.ai");
    }

    /* Extract params */
    const char *prompt = json_get_str(args, "prompt", "");
    const char *operation = json_get_str(args, "operation", "generate");
    const char *aspect_ratio = json_get_str(args, "aspect_ratio", "16:9");
    const char *resolution = json_get_str(args, "resolution", "720p");
    int duration = (int)json_get_num(args, "duration", 5);
    const char *image_url = json_get_str(args, "image_url", NULL);
    const char *video_url = json_get_str(args, "video_url", NULL);
    const char *negative_prompt = json_get_str(args, "negative_prompt", NULL);
    int seed = (int)json_get_num(args, "seed", 0);
    bool has_audio = json_get_bool(args, "audio", false);

    /* Prompt is required for generate */
    if (strcmp(operation, "generate") == 0 && (!prompt || !*prompt)) {
        json_free(args);
        return fal_error_response("Missing 'prompt' for generate operation");
    }

    /* Determine API endpoint */
    const char *api_url = FAL_VIDEO_BASE;
    if (strcmp(operation, "extend") == 0 || strcmp(operation, "edit") == 0) {
        api_url = FAL_VIDEO_EXTEND;
    }

    /* Build request body — start with required fields */
    char body[16384];
    size_t pos = 0;
    size_t rem = sizeof(body);
    int n;

    n = snprintf(body + pos, rem, "{\"prompt\":\"");
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Use shared JSON escape helper */
    {
        char esc_prompt[4096];
        fal_escape_json(prompt, esc_prompt, sizeof(esc_prompt));
        n = snprintf(body + pos, rem, "%s", esc_prompt);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    n = snprintf(body + pos, rem, "\",\"aspect_ratio\":\"%s\"", aspect_ratio);
    if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }

    /* Optional fields */
    if (duration > 0) {
        n = snprintf(body + pos, rem, ",\"duration\":%d", duration);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (image_url && *image_url) {
        n = snprintf(body + pos, rem, ",\"image_url\":\"%s\"", image_url);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (video_url && *video_url) {
        n = snprintf(body + pos, rem, ",\"video_url\":\"%s\"", video_url);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (negative_prompt && *negative_prompt) {
        n = snprintf(body + pos, rem, ",\"negative_prompt\":\"%s\"", negative_prompt);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (seed > 0) {
        n = snprintf(body + pos, rem, ",\"seed\":%d", seed);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    const char *model = json_get_str(args, "model", NULL);
    if (model && *model) {
        n = snprintf(body + pos, rem, ",\"model\":\"%s\"", model);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (strcmp(resolution, "720p") != 0) {
        n = snprintf(body + pos, rem, ",\"resolution\":\"%s\"", resolution);
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    if (has_audio) {
        n = snprintf(body + pos, rem, ",\"audio\":true");
        if (n > 0 && (size_t)n < rem) { pos += n; rem -= n; }
    }

    /* Close JSON */
    n = snprintf(body + pos, rem, "}");
    if (n >= 0 && (size_t)n < rem) { pos += n; }

    /* Use shared FAL POST helper with 120s timeout (video gen is slow) */
    http_resp_t *resp = fal_post_json(api_url, body, 120);

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

    /* Extract video URL from response */
    json_t *videos = json_obj_get(result, "video");
    const char *video_url_out = NULL;
    if (videos) {
        video_url_out = json_get_str(videos, "url", NULL);
    }
    if (!video_url_out) {
        videos = json_obj_get(result, "videos");
        if (videos && json_len(videos) > 0) {
            json_t *first = json_get(videos, 0);
            if (first) video_url_out = json_get_str(first, "url", NULL);
        }
    }

    if (!video_url_out) {
        /* Check for direct url field */
        video_url_out = json_get_str(result, "url", NULL);
    }

    if (!video_url_out) {
        /* Fallback: return the entire response for inspection */
        char *serialized = json_serialize(result);
        char err[2048];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"No video URL in response\",\"response\":%s}",
            serialized ? serialized : "null");
        free(serialized);
        json_free(result);
        json_free(args);
        return strdup(err);
    }

    /* Build successful response */
    char out[8192];
    snprintf(out, sizeof(out),
        "{\"success\":true,\"video\":\"%s\",\"operation\":\"%s\"}",
        video_url_out, operation);

    json_free(result);
    json_free(args);
    return strdup(out);
}

/* ================================================================
 *  Registration
 * ================================================================ */

/* FAL provider availability check */
static bool fal_video_is_available(void) {
    return fal_get_api_key() != NULL;
}

void registry_init_video_gen(void) {
    /* Register FAL provider in the video_gen registry */
    video_gen_provider_t fal_provider;
    memset(&fal_provider, 0, sizeof(fal_provider));
    snprintf(fal_provider.name, sizeof(fal_provider.name), "%s", "fal");
    snprintf(fal_provider.display_name, sizeof(fal_provider.display_name), "%s", "FAL.ai (Veo3)");
    fal_provider.is_available = fal_video_is_available;
    fal_provider.generate = NULL; /* Uses video_generate_handler directly */
    video_gen_register_provider(&fal_provider);

    registry_register("video_generate",
        "Generate video from text or images using FAL.ai Veo3 API. "
        "Supports text-to-video (generate), image-to-video (with image_url), "
        "and video extension.",
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
        "  \"prompt\":{\"type\":\"string\",\"description\":\"Text description of the video to generate\"},"
        "  \"operation\":{\"type\":\"string\",\"description\":\"Operation type: generate, edit, extend\",\"enum\":[\"generate\",\"edit\",\"extend\"],\"default\":\"generate\"},"
        "  \"aspect_ratio\":{\"type\":\"string\",\"description\":\"Aspect ratio (16:9, 9:16, 1:1)\",\"default\":\"16:9\"},"
        "  \"duration\":{\"type\":\"integer\",\"description\":\"Target duration in seconds\",\"default\":5},"
        "  \"image_url\":{\"type\":\"string\",\"description\":\"Source image URL for image-to-video\"},"
        "  \"video_url\":{\"type\":\"string\",\"description\":\"Source video URL for edit/extend operations\"},"
        "  \"resolution\":{\"type\":\"string\",\"description\":\"Output resolution (720p, 1080p)\",\"default\":\"720p\"},"
        "  \"negative_prompt\":{\"type\":\"string\",\"description\":\"What to avoid in the video\"},"
        "  \"audio\":{\"type\":\"boolean\",\"description\":\"Generate with audio track\",\"default\":false},"
        "  \"seed\":{\"type\":\"integer\",\"description\":\"Random seed for reproducibility\"},"
        "  \"model\":{\"type\":\"string\",\"description\":\"Model override (e.g., fal-ai/veo3, fal-ai/video-consistency)\"}"
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        video_generate_handler);
}
