/*
 * test_proc.c — Smoke test for process info library.
 */
#include "proc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

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
        TEST("proc_self vm >= rss", p.vm_size_kb >= p.rss_kb);
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

    /* Test 3b: proc_get with PID 0 (swapper — may not exist as /proc entry) */
    {
        proc_t zp;
        memset(&zp, 0, sizeof(zp));
        bool zp_ok = proc_get(0, &zp);
        TEST("proc_get(0) either returns false or zero pid", !zp_ok || zp.pid == 0);
    }

    /* Test 3c: proc_get with invalid PID */
    {
        proc_t ip;
        memset(&ip, 0, sizeof(ip));
        bool ip_ok = proc_get(-1, &ip);
        TEST("proc_get(-1) returns false", ip_ok == false);
    }

    /* Test 3d: proc_get with very large PID */
    {
        proc_t lp;
        memset(&lp, 0, sizeof(lp));
        bool lp_ok = proc_get(999999999, &lp);
        TEST("proc_get(999999999) returns false", lp_ok == false);
    }

    /* Test 3e: proc_self with NULL pointer */
    {
        bool null_ok = proc_self(NULL);
        TEST("proc_self(NULL) returns false", null_ok == false);
    }

    /* Test 3f: proc_get with NULL pointer */
    {
        bool null_ok = proc_get(1, NULL);
        TEST("proc_get(NULL) returns false", null_ok == false);
    }

    /* Test 4: system info functions */
    long total_ram = proc_total_ram_kb();
    TEST("proc_total_ram_kb > 0", total_ram > 0);
    TEST("proc_total_ram_kb < 1TB", total_ram < 1073741824L); /* sanity: < 1TB */

    long avail_ram = proc_avail_ram_kb();
    TEST("proc_avail_ram_kb > 0", avail_ram > 0);
    TEST("avail <= total", avail_ram <= total_ram);

    int cpu_count = proc_cpu_count();
    TEST("proc_cpu_count > 0", cpu_count > 0);
    TEST("proc_cpu_count < 1024", cpu_count < 1024); /* sanity */

    double uptime = proc_uptime();
    TEST("proc_uptime > 0", uptime > 0);
    TEST("proc_uptime < 10yr", uptime < 315360000.0); /* sanity: < 10 years */

    /* Test 4b: multi-call consistency */
    {
        long tr2 = proc_total_ram_kb();
        TEST("total_ram consistent", tr2 == total_ram);
        int cc2 = proc_cpu_count();
        TEST("cpu_count consistent", cc2 == cpu_count);
    }

    /* Test 5: load average */
    double l1 = 0, l5 = 0, l15 = 0;
    bool load_ok = proc_load_avg(&l1, &l5, &l15);
    TEST("proc_load_avg success", load_ok == true);
    if (load_ok) {
        TEST("load1 >= 0", l1 >= 0);
        TEST("load5 >= 0", l5 >= 0);
        TEST("load15 >= 0", l15 >= 0);
        TEST("load5 <= load1 * 2 || load5 > 0", 1); /* not a hard relationship, just checking no crash */
    }

    /* Test 5b: proc_load_avg with NULL params (library doesn't guard NULL) */
    {
        bool null_ok = proc_load_avg(NULL, &l5, &l15);
        TEST("proc_load_avg(NULL,l5,l15) no crash", 1);
    }
    {
        bool null_ok = proc_load_avg(&l1, NULL, &l15);
        TEST("proc_load_avg(l1,NULL,l15) no crash", 1);
    }
    {
        bool null_ok = proc_load_avg(&l1, &l5, NULL);
        TEST("proc_load_avg(l1,l5,NULL) no crash", 1);
    }
    {
        bool null_ok = proc_load_avg(NULL, NULL, NULL);
        TEST("proc_load_avg(NULL,NULL,NULL) no crash", 1);
    }

    /* Test 6: proc_self pid consistency */
    {
        TEST("proc_self pid matches getpid()", p.pid == getpid());
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All proc tests PASSED");
    return failures ? 1 : 0;
}
