#ifndef HERMES_BINARY_H
#define HERMES_BINARY_H

/*
 * binary.h — Binary file extension detection for Hermes C.
 * Port of Python tools/binary_extensions.py.
 *
 * Checks if a file path has a binary (non-text) extension.
 * Used to skip binary files for text-based operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Check if a file path has a binary extension.
 * Pure string check — no I/O.
 *
 * @param path  File path to check (full path or just filename).
 * @return true if the path ends with a recognized binary extension.
 */
bool has_binary_extension(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_BINARY_H */
