#ifndef HERMES_SANDBOX_H
#define HERMES_SANDBOX_H

#include <stdbool.h>
#include <stddef.h>

/* O14: Sandbox escape detection system.
 * Monitors command strings for escape patterns before execution.
 * Integrated into terminal and exec_code tool handlers. */

#define SANDBOX_ESCAPE_REASON_MAX 256

/* Result of an escape check */
typedef struct {
    bool blocked;                        /* True if escape attempt detected */
    char reason[SANDBOX_ESCAPE_REASON_MAX]; /* Human-readable reason */
} sandbox_escape_result_t;

/* Initialize sandbox escape detection with default patterns.
 * Called once at startup from tool_init. */
void sandbox_escape_init(void);

/* Enable/disable escape checking (default: enabled) */
void sandbox_escape_enable(bool enabled);
bool sandbox_escape_is_enabled(void);

/* Check a command string for escape patterns.
 * Returns result with blocked=false if safe, blocked=true with reason if escape detected.
 * cmd: the command string to check
 * len: length of cmd, or -1 for strlen(cmd)
 * context: "terminal", "exec_code", "file" — used in log messages */
sandbox_escape_result_t sandbox_escape_check(const char *cmd, int len, const char *context);

/* Check a file path for escape patterns (path traversal, sensitive paths).
 * Separate from sandbox_check_path — this catches escape patterns
 * that the file sandbox might miss (proc/sys access, etc.). */
sandbox_escape_result_t sandbox_escape_check_path(const char *path, const char *context);

/* Add a custom escape pattern to the detection set.
 * pattern: substring to detect (case-sensitive)
 * reason: description for audit log
 * Returns true if added, false if pattern table full. */
bool sandbox_escape_add_pattern(const char *pattern, const char *reason);

/* Number of escape attempts logged (for audit/monitoring) */
int sandbox_escape_attempt_count(void);

/* Get the total count of blocked escape attempts */
int sandbox_escape_blocked_count(void);

/* Internal pattern flags returned by the matcher */
#define SANDBOX_FLAG_NONE        0
#define SANDBOX_FLAG_PATH          (1<<0)  /* Sensitive file path */
#define SANDBOX_FLAG_INJECTION     (1<<1)  /* Shell injection */
#define SANDBOX_FLAG_PROC_SYS      (1<<2)  /* /proc/ or /sys/ access */
#define SANDBOX_FLAG_ESCAPE_CMD    (1<<3)  /* bwrap/nsenter/docker escape */
#define SANDBOX_FLAG_ENV_POISON    (1<<4)  /* LD_PRELOAD, PATH poisoning */
#define SANDBOX_FLAG_FORK_BOMB     (1<<5)  /* Fork bomb pattern */
#define SANDBOX_FLAG_NET_ACCESS    (1<<6)  /* Network-related escape */
#define SANDBOX_FLAG_CUSTOM        (1<<7)  /* User-defined pattern */

#endif /* HERMES_SANDBOX_H */
