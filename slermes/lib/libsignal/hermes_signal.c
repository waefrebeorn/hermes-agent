/*
 * signal.c — C signal handling helpers (J13: Python signal port).
 *
 * Uses standard C signal() for portability. No POSIX sigaction dependency.
 */
#define _POSIX_C_SOURCE 199309L

#include "hermes_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* ================================================================
 *  Public API
 * ================================================================ */

bool signal_on(int signum, signal_handler_t handler) {
    void (*prev)(int) = signal(signum, handler);
    return prev != SIG_ERR;
}

bool signal_default(int signum) {
    void (*prev)(int) = signal(signum, SIG_DFL);
    return prev != SIG_ERR;
}

/* Callback for common handlers */
static volatile int *g_stop = NULL;

static void stop_handler(int signum) {
    (void)signum;
    if (g_stop) *g_stop = 1;
}

bool signal_register_common(volatile int *stop_flag) {
    g_stop = stop_flag;
    return signal_on(SIGINT, stop_handler) && signal_on(SIGTERM, stop_handler);
}

void signal_safe_write(const char *msg) {
    if (!msg) return;
    size_t len = strlen(msg);
    (void)write(STDERR_FILENO, msg, len);
}

void signal_default_handler(int signum) {
    const char *name = NULL;
    switch (signum) {
        case SIGINT:  name = "SIGINT";  break;
        case SIGTERM: name = "SIGTERM"; break;
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
        case SIGFPE:  name = "SIGFPE";  break;
        case SIGILL:  name = "SIGILL";  break;
        case SIGPIPE: name = "SIGPIPE"; break;
        case SIGBUS:  name = "SIGBUS";  break;
        default:      name = "UNKNOWN"; break;
    }
    signal_safe_write("signal: ");
    signal_safe_write(name);
    signal_safe_write("\n");
    _exit(128 + signum);
}
