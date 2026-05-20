/*
 * test_proc.c — Smoke test for process info library.
 */
#include "proc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: proc_self */
    proc_t p;
    memset(&p, 0, sizeof(p));
    bool ok = proc_self(&p);
    TEST("proc_self success", ok == true);
    if (ok) {
        TEST("proc_self pid > 0", p.pid > 0);
        TEST("proc_self rss_kb > 0", p.rss_kb > 0);
        TEST("proc_self vm_size_kb > 0", p.vm_size_kb > 0);
        TEST("proc_self name not empty", strlen(p.name) > 0);
    }

    /* Test 2: proc_get for PID 1 (init) */
    proc_t init;
    memset(&init, 0, sizeof(init));
    bool init_ok = proc_get(1, &init);
    TEST("proc_get(1) success", init_ok == true);
    if (init_ok) {
        TEST("proc_get pid=1", init.pid == 1);
        TEST("proc_get name non-empty", strlen(init.name) > 0);
    }

    /* Test 3: get this process info via proc_get */
    proc_t p2;
    memset(&p2, 0, sizeof(p2));
    bool p2_ok = proc_get(p.pid, &p2);
    TEST("proc_get own PID", p2_ok == true);
    if (p2_ok) {
        TEST("proc_get pid matches", p2.pid == p.pid);
    }

    /* Test 4: system info functions */
    long total_ram = proc_total_ram_kb();
    TEST("proc_total_ram_kb > 0", total_ram > 0);

    long avail_ram = proc_avail_ram_kb();
    TEST("proc_avail_ram_kb > 0", avail_ram > 0);

    int cpu_count = proc_cpu_count();
    TEST("proc_cpu_count > 0", cpu_count > 0);

    double uptime = proc_uptime();
    TEST("proc_uptime > 0", uptime > 0);

    /* Test 5: load average */
    double l1, l5, l15;
    bool load_ok = proc_load_avg(&l1, &l5, &l15);
    TEST("proc_load_avg success", load_ok == true);
    if (load_ok) {
        TEST("load1 >= 0", l1 >= 0);
        TEST("load5 >= 0", l5 >= 0);
        TEST("load15 >= 0", l15 >= 0);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All proc tests PASSED");
    return failures ? 1 : 0;
}
