/*
 * video_gen.c — Video generation tool for Hermes C.
 * Uses FAL.ai REST API for text-to-video, image-to-video, and video editing.
 * Reads FAL_API_KEY from config/env (shared with image_gen).
 *
 * Mirrors Python tools/video_generation_tool.py with a single unified
 * tool that dispatches to FAL.ai video endpoints.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_http.h"
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

    /* Get API key (shared with image_gen) */
    const char *api_key = getenv("FAL_API_KEY");
    if (!api_key || !*api_key)
        api_key = getenv("SLERMES_FAL_KEY");
    if (!api_key || !*api_key) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"FAL_API_KEY not set. Get a key at https://fal.ai\"}");
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
        return strdup("{\"success\":false,\"error\":\"Missing 'prompt' for generate operation\"}");
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

    /* Escape prompt string */
    for (const char *sp = prompt; *sp && rem > 4; sp++) {
        if (*sp == '"' || *sp == '\\') {
            body[pos++] = '\\'; body[pos++] = *sp; rem -= 2;
        } else if (*sp == '\n') {
            body[pos++] = '\\'; body[pos++] = 'n'; rem -= 2;
        } else {
            body[pos++] = *sp; rem--;
        }
    }
    body[pos] = '\0';

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

    /* Create HTTP client and make POST request */
    http_t *h = http_new(120);  /* Video gen can take 60-120s */
    if (!h) {
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to create HTTP client\"}");
    }

    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Key %s\r\n", api_key);

    http_resp_t *resp = http_post_json_auth(h, api_url, body, auth_hdr);

    if (!resp) {
        http_free(h);
        json_free(args);
        return strdup("{\"success\":false,\"error\":\"Failed to connect to FAL API\"}");
    }

    if (resp->status != 200) {
        char err[1536];
        snprintf(err, sizeof(err),
            "{\"success\":false,\"error\":\"FAL video API returned HTTP %d: %s\"}",
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

void registry_init_video_gen(void) {
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
        "  \"seed\":{\"type\":\"integer\",\"description\":\"Random seed for reproducibility\"}"
        "},"
        "\"required\":[\"prompt\"]"
        "}",
        video_generate_handler);
}
