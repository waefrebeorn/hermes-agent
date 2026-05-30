/**
 * @file hermes_yuanbao_media.h
 * @brief Yuanbao media attachment helpers.
 *
 * Port of Python gateway/platforms/yuanbao_media.py utility functions.
 * Provides: file ID generation, media message body builders.
 *
 * MIT License — WuBu Slermes Project
 */
#ifndef HERMES_YUANBAO_MEDIA_H
#define HERMES_YUANBAO_MEDIA_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate a random file ID (32 hex characters from 16 random bytes).
 * Port of Python generate_file_id() — secrets.token_hex(16).
 * Returns malloc'd string, caller must free(). NULL on error.
 */
char *yuanbao_generate_file_id(void);

/**
 * Build a TIMImageElem message body JSON string.
 * Port of Python build_image_msg_body().
 *
 * @param url       Image public URL
 * @param uuid      File UUID (MD5 hex or other identifier), may be NULL
 * @param filename  Filename (used when uuid is NULL), may be NULL
 * @param size      File size in bytes
 * @param width     Image width in pixels
 * @param height    Image height in pixels
 * @param mime_type MIME type (used to determine image_format), may be NULL
 * @return malloc'd JSON string, caller must free(). NULL on error.
 */
char *yuanbao_build_image_msg(const char *url,
                              const char *uuid,
                              const char *filename,
                              int size,
                              int width,
                              int height,
                              const char *mime_type);

/**
 * Build a TIMFileElem message body JSON string.
 * Port of Python build_file_msg_body().
 *
 * @param url      File public URL
 * @param filename Filename (with extension)
 * @param uuid     File UUID (may be NULL, falls back to filename)
 * @param size     File size in bytes
 * @return malloc'd JSON string, caller must free(). NULL on error.
 */
char *yuanbao_build_file_msg(const char *url,
                             const char *filename,
                             const char *uuid,
                             int size);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_YUANBAO_MEDIA_H */
