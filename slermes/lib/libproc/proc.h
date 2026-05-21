#ifndef LIBPROC_H
#define LIBPROC_H

/*
 * libproc.h — Process monitoring via /proc filesystem.
 * Zero external deps. Replaces Python's psutil for Linux.
 *
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   proc_t p = {0};
 *   proc_self(&p);
 *   printf("PID: %d, RSS: %zu KB\n", p.pid, p.rss_kb);
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   pid;
    char  name[256];         /* process name */
    long  rss_kb;            /* resident set size (KB) */
    long  vm_size_kb;        /* virtual memory size (KB) */
    double cpu_percent;      /* CPU usage since last call */
    unsigned long utime;     /* user jiffies */
    unsigned long stime;     /* system jiffies */
    long  uptime_ticks;      /* system uptime in jiffies */
} proc_t;

/* Get info for current process */
bool proc_self(proc_t *p);

/* Get info for a specific PID */
bool proc_get(int pid, proc_t *p);

/* Read a single value from /proc/pid/stat */
bool proc_read_stat(int pid, proc_t *p);

/* Read memory info from /proc/pid/status */
bool proc_read_status(int pid, proc_t *p);

/* Get total system memory (KB) */
long proc_total_ram_kb(void);

/* Get available system memory (KB) */
long proc_avail_ram_kb(void);

/* Get CPU count (online) */
int proc_cpu_count(void);

/* Get system uptime in seconds */
double proc_uptime(void);

/* Get load average (1, 5, 15 min) */
bool proc_load_avg(double *load1, double *load5, double *load15);

#ifdef __cplusplus
}
#endif

#endif /* LIBPROC_H */
