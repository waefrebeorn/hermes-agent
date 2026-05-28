#ifndef HERMES_ANSI_STRIP_H
#define HERMES_ANSI_STRIP_H

/*
 * ansi_strip.h — Strip ANSI escape sequences from text.
 * Port of Python tools/ansi_strip.py.
 *
 * Implements the full ECMA-48 spec: CSI (including private-mode ? prefix,
 * colon-separated params, intermediate bytes), OSC (BEL and ST terminators),
 * DCS/SOS/PM/APC string sequences, nF multi-byte escapes, Fp/Fe/Fs
 * single-byte escapes, 8-bit C1 control characters.
 *
 * Uses a byte-by-byte state machine — no regex overhead.
 *
 * Usage:
 *   char *clean = ansi_strip("hello \033[31mworld\033[0m");
 *   // clean == "hello world"
 *   free(clean);
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Strip ANSI escape sequences from a NUL-terminated string.
 * Returns a malloc'd string with all ANSI sequences removed.
 * On NULL input returns NULL.
 * On allocation failure returns NULL (caller should check). */
char *ansi_strip(const char *text);

/* Same as ansi_strip() but writes into a caller-provided buffer.
 * buf must be at least (strlen(text) + 1) bytes.
 * Returns buf on success, NULL on failure.
 * Safe to call with buf == text (in-place stripping). */
char *ansi_strip_buf(const char *text, char *buf, size_t buf_size);

/* Fast-path check: returns true if text contains any ESC or C1 byte.
 * Useful when you want to skip the full strip on clean text. */
bool ansi_has_escape(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ANSI_STRIP_H */
