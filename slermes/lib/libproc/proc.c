/*
 * proc.c — Process monitoring via /proc for Linux.
 * Reads /proc/[pid]/stat, /proc/[pid]/status, /proc/meminfo, /proc/loadavg.
 * MIT License — WuBu Hermes Project
 */

#include "proc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* ================================================================
 *  Helpers
 * ================================================================ */

/* Read file content into fixed buffer. Returns true on success. */
static bool read_file(const char *path, char *buf, size_t bufsz) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, bufsz - 1, f);
    fclose(f);
    buf[n] = '\0';
    return n > 0;
}

/* ================================================================
 *  /proc/self/stat parser
 *  Format: pid (comm) state ppid pgrp session tty_nr tpgid flags
 *          minflt cminflt majflt cmajflt utime stime cutime cstime
 *          priority nice num_threads itrealvalue starttime vsize rss
 * ================================================================ */

bool proc_read_stat(int pid, proc_t *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    char buf[4096];
    if (!read_file(path, buf, sizeof(buf))) return false;

    /* Parse: PID (comm) state ... */
    const char *ptr = buf;
    p->pid = atoi(ptr);

    /* Skip to first '(' */
    ptr = strchr(ptr, '(');
    if (!ptr) return false;
    ptr++; /* skip '(' */

    /* Extract name until ')' */
    const char *paren = strchr(ptr, ')');
    if (!paren) return false;
    size_t name_len = (size_t)(paren - ptr);
    if (name_len >= sizeof(p->name)) name_len = sizeof(p->name) - 1;
    memcpy(p->name, ptr, name_len);
    p->name[name_len] = '\0';

    ptr = paren + 2; /* skip ") " */

    /* Skip state, ppid, pgrp, session, tty, tpgid, flags (7 fields) */
    int fields_to_skip = 7;
    for (int i = 0; i < fields_to_skip; i++) {
        while (*ptr && *ptr != ' ') ptr++;
        while (*ptr == ' ') ptr++;
    }

    /* Skip minflt, cminflt, majflt, cmajflt (4 fields) */
    for (int i = 0; i < 4; i++) {
        while (*ptr && *ptr != ' ') ptr++;
        while (*ptr == ' ') ptr++;
    }

    /* utime and stime */
    {
        char *end = NULL;
        p->utime = strtoul(ptr, &end, 10);
        if (end) ptr = end;
    }
    while (*ptr == ' ') ptr++;
    {
        char *end = NULL;
        p->stime = strtoul(ptr, &end, 10);
        if (end) ptr = end;
    }
    while (*ptr == ' ') ptr++;  /* skip to next field */

    /* Skip cutime, cstime, priority, nice, num_threads, itrealvalue (6 fields) */
    for (int i = 0; i < 6; i++) {
        while (*ptr && *ptr != ' ') ptr++;
        while (*ptr == ' ') ptr++;
    }

    /* starttime */
    {
        char *end = NULL;
        unsigned long starttime_val = strtoul(ptr, &end, 10);
        (void)starttime_val;
        if (end && end != ptr) ptr = end;
    }
    while (*ptr == ' ') ptr++;

    /* vsize (virtual memory size in bytes) */
    long vsize = 0;
    {
        char *end = NULL;
        vsize = strtol(ptr, &end, 10);
        if (end) ptr = end;
        p->vm_size_kb = vsize / 1024;
    }
    while (*ptr == ' ') ptr++;

    /* rss (number of pages) */
    {
        char *end = NULL;
        long rss_pages = strtol(ptr, &end, 10);
        if (end) ptr = end;
        p->rss_kb = rss_pages * 4; /* assume 4KB pages */
    }

    return true;
}

/* ================================================================
 *  /proc/pid/status parser
 * ================================================================ */

bool proc_read_status(int pid, proc_t *p) {
    (void)p;
    /* Memory info is read from stat above — status file gives us
     * additional details. For now, stat provides what we need. */
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    char buf[4096];
    if (!read_file(path, buf, sizeof(buf))) return false;

    /* Parse VmRSS if available for more accurate RSS */
    const char *vmrss = strstr(buf, "VmRSS:");
    if (vmrss) {
        const char *val = vmrss + 7;
        while (*val && (*val == ' ' || *val == '\t')) val++;
        p->rss_kb = atol(val);
    }

    return true;
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool proc_self(proc_t *p) {
    return proc_get(getpid(), p);
}

bool proc_get(int pid, proc_t *p) {
    if (!p) return false;
    memset(p, 0, sizeof(*p));
    p->pid = pid;

    if (!proc_read_stat(pid, p)) return false;
    proc_read_status(pid, p); /* best-effort, rss may be more accurate */

    /* Read /proc/stat for total CPU jiffies (for cpu_percent calculation) */
    char buf[4096];
    if (read_file("/proc/stat", buf, sizeof(buf))) {
        unsigned long long total = 0;
        /* Parse first line: cpu  user nice system idle ... */
        const char *cpu_line = buf;
        if (strncmp(cpu_line, "cpu ", 4) == 0) {
            const char *val = cpu_line + 4;
            for (int i = 0; i < 10; i++) {
                while (*val == ' ') val++;
                total += strtoull(val, (char **)&val, 10);
            }
        }
        p->uptime_ticks = (long)total;
    }

    return true;
}

/* ================================================================
 *  System info
 * ================================================================ */

long proc_total_ram_kb(void) {
    char buf[4096];
    if (!read_file("/proc/meminfo", buf, sizeof(buf))) return 0;
    const char *memtotal = strstr(buf, "MemTotal:");
    if (!memtotal) return 0;
    const char *val = memtotal + 9;
    while (*val && (*val == ' ' || *val == '\t')) val++;
    return atol(val);
}

long proc_avail_ram_kb(void) {
    char buf[4096];
    if (!read_file("/proc/meminfo", buf, sizeof(buf))) return 0;
    const char *memavail = strstr(buf, "MemAvailable:");
    if (!memavail) return 0;
    const char *val = memavail + 13;
    while (*val && (*val == ' ' || *val == '\t')) val++;
    return atol(val);
}

int proc_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

double proc_uptime(void) {
    char buf[256];
    if (!read_file("/proc/uptime", buf, sizeof(buf))) return 0;
    return atof(buf);
}

bool proc_load_avg(double *load1, double *load5, double *load15) {
    char buf[256];
    if (!read_file("/proc/loadavg", buf, sizeof(buf))) return false;
    if (load1) *load1 = atof(buf);
    const char *p = strchr(buf, ' ');
    if (p && load5) { p++; *load5 = atof(p); }
    p = strchr(p ? p : buf, ' ');
    if (p && load15) { p++; *load15 = atof(p); }
    return true;
}
