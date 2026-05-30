/*
 * yuanbao_media.c — Yuanbao media attachment helpers for Hermes C.
 *
 * Port of Python gateway/platforms/yuanbao_media.py.
 * Provides: generate_file_id(), build_image_msg_body(), build_file_msg_body().
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_yuanbao_media.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  generate_file_id — 32 hex chars from 16 random bytes
 *  Port of Python yuanbao_media.generate_file_id().
 * ================================================================ */

char *yuanbao_generate_file_id(void)
{
    unsigned char buf[16];
    if (!crypto_random_bytes(buf, 16))
        return NULL;
    return crypto_hex_encode(buf, 16);
}

/* ================================================================
 *  build_image_msg — TIMImageElem JSON string builder
 *  Port of Python yuanbao_media.build_image_msg_body().
 * ================================================================ */

char *yuanbao_build_image_msg(const char *url,
                              const char *uuid,
                              const char *filename,
                              int size,
                              int width,
                              int height,
                              const char *mime_type)
{
    if (!url) return NULL;

    /* Determine uuid: explicit uuid > filename > url basename > "image" */
    const char *resolved_uuid = uuid;
    if (!resolved_uuid || !resolved_uuid[0])
        resolved_uuid = filename;
    if (!resolved_uuid || !resolved_uuid[0]) {
        char *basename = url_extract_basename(url);
        resolved_uuid = basename;
        if (!resolved_uuid || !resolved_uuid[0])
            resolved_uuid = "image";
    }

    /* Determine image_format from mime_type */
    int image_format = 255; /* default: unknown */
    if (mime_type && mime_type[0])
        image_format = (int)url_get_image_format(mime_type);

    /* Build JSON */
    json_t *info = json_object();
    json_set(info, "type", json_number(1));
    json_set(info, "size", json_number(size));
    json_set(info, "width", json_number(width));
    json_set(info, "height", json_number(height));
    json_set(info, "url", json_string(url));

    json_t *info_arr = json_array();
    json_append(info_arr, info);

    json_t *content = json_object();
    json_set(content, "uuid", json_string(resolved_uuid));
    json_set(content, "image_format", json_number(image_format));
    json_set(content, "image_info_array", info_arr);

    json_t *msg = json_object();
    json_set(msg, "msg_type", json_string("TIMImageElem"));
    json_set(msg, "msg_content", content);

    json_t *root = json_array();
    json_append(root, msg);

    char *result = json_serialize(root);
    json_free(root);
    return result;
}

/* ================================================================
 *  build_file_msg — TIMFileElem JSON string builder
 *  Port of Python yuanbao_media.build_file_msg_body().
 * ================================================================ */

char *yuanbao_build_file_msg(const char *url,
                             const char *filename,
                             const char *uuid,
                             int size)
{
    if (!url || !filename) return NULL;

    /* uuid falls back to filename */
    const char *resolved_uuid = uuid;
    if (!resolved_uuid || !resolved_uuid[0])
        resolved_uuid = filename;

    json_t *content = json_object();
    json_set(content, "uuid", json_string(resolved_uuid));
    json_set(content, "file_name", json_string(filename));
    json_set(content, "file_size", json_number(size));
    json_set(content, "url", json_string(url));

    json_t *msg = json_object();
    json_set(msg, "msg_type", json_string("TIMFileElem"));
    json_set(msg, "msg_content", content);

    json_t *root = json_array();
    json_append(root, msg);

    char *result = json_serialize(root);
    json_free(root);
    return result;
}
