#ifndef HERMES_TIRITH_H
#define HERMES_TIRITH_H

/*
 * tirith.h — Pre-exec security scanning for Hermes C.
 * Mirrors Python tools/tirith_security.py: external binary + inline checks.
 * Scans commands for: homograph URLs, pipe-to-interpreter, terminal injection.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Tirith verdict */
typedef enum {
    TIRITH_ALLOW = 0,
    TIRITH_BLOCK = 1,
    TIRITH_WARN  = 2,
    TIRITH_ERROR = -1,  /* Operational failure */
} tirith_verdict_t;

/* Run tirith binary on command. Returns verdict. */
tirith_verdict_t tirith_scan(const char *command);

/* Inline check: pipe-to-interpreter patterns (| sh, | bash, etc.) */
bool tirith_has_pipe_to_interpreter(const char *command);

/* Inline check: homograph/mixed-script URLs in command */
bool tirith_has_suspicious_url(const char *command);

/* Inline check: backtick or $() command substitution */
bool tirith_has_command_substitution(const char *command);

/* Inline check: environment variable expansion in shell args */
bool tirith_has_env_injection(const char *command);

/* Full inline security scan (all above checks). Returns TIRITH_BLOCK if any fail. */
tirith_verdict_t tirith_inline_scan(const char *command);

/* Set tirith binary path. NULL = use default ("tirith" on PATH). */
void tirith_set_path(const char *path);

/* Enable/disable tirith binary scanning. Inline checks always run. */
void tirith_set_enabled(bool enabled);

/* Set fail-open mode: on binary failure, allow instead of block. */
void tirith_set_fail_open(bool fail_open);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TIRITH_H */
