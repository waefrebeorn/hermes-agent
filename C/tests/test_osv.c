/*
 * test_osv.c — Tests for OSV malware check library.
 * Port of Python tools/osv_check.py tests.
 */

#include "osv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests = 0;
static int passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        printf("  FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
    } else { \
        passed++; \
    } \
} while(0)

/* ─── Ecosystem Detection Tests ─────────────────────────── */

static void test_infer_ecosystem(void)
{
    TEST("npx → npm", strcmp(osv_infer_ecosystem("npx"), "npm") == 0);
    TEST("npx.cmd → npm", strcmp(osv_infer_ecosystem("npx.cmd"), "npm") == 0);
    TEST("NPX uppercase → npm", strcmp(osv_infer_ecosystem("NPX"), "npm") == 0);
    TEST("full path npx → npm", strcmp(osv_infer_ecosystem("/usr/bin/npx"), "npm") == 0);
    TEST("uvx → PyPI", strcmp(osv_infer_ecosystem("uvx"), "PyPI") == 0);
    TEST("uvx.cmd → PyPI", strcmp(osv_infer_ecosystem("uvx.cmd"), "PyPI") == 0);
    TEST("pipx → PyPI", strcmp(osv_infer_ecosystem("pipx"), "PyPI") == 0);
    TEST("unknown cmd → NULL", osv_infer_ecosystem("curl") == NULL);
    TEST("docker → NULL", osv_infer_ecosystem("docker") == NULL);
    TEST("NULL → NULL", osv_infer_ecosystem(NULL) == NULL);
    TEST("empty → NULL", osv_infer_ecosystem("") == NULL);
}

/* ─── NPM Package Parsing Tests ─────────────────────────── */

static void test_parse_npm_unscoped(void)
{
    const char *args[] = {"some-package"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "npm", &name, &ver);
    TEST("npm unscoped name", ret == 0 && name && strcmp(name, "some-package") == 0);
    TEST("npm unscoped no version", ver == NULL);
    free(name); free(ver);
}

static void test_parse_npm_version(void)
{
    const char *args[] = {"package@1.2.3"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "npm", &name, &ver);
    TEST("npm with version name", ret == 0 && name && strcmp(name, "package") == 0);
    TEST("npm with version ver", ver && strcmp(ver, "1.2.3") == 0);
    free(name); free(ver);
}

static void test_parse_npm_latest(void)
{
    const char *args[] = {"package@latest"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "npm", &name, &ver);
    TEST("npm @latest name", ret == 0 && name && strcmp(name, "package") == 0);
    TEST("npm @latest no version", ver == NULL);
    free(name); free(ver);
}

static void test_parse_npm_scoped(void)
{
    const char *args[] = {"@scope/package"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "npm", &name, &ver);
    TEST("npm scoped name", ret == 0 && name && strcmp(name, "@scope/package") == 0);
    TEST("npm scoped no version", ver == NULL);
    free(name); free(ver);
}

static void test_parse_npm_scoped_version(void)
{
    const char *args[] = {"@scope/package@2.0.0"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "npm", &name, &ver);
    TEST("npm scoped+version name", ret == 0 && name && strcmp(name, "@scope/package") == 0);
    TEST("npm scoped+version ver", ver && strcmp(ver, "2.0.0") == 0);
    free(name); free(ver);
}

/* ─── PyPI Package Parsing Tests ──────────────────────────── */

static void test_parse_pypi_simple(void)
{
    const char *args[] = {"requests"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "PyPI", &name, &ver);
    TEST("pypi simple name", ret == 0 && name && strcmp(name, "requests") == 0);
    TEST("pypi simple no version", ver == NULL);
    free(name); free(ver);
}

static void test_parse_pypi_version(void)
{
    const char *args[] = {"requests==2.31.0"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "PyPI", &name, &ver);
    TEST("pypi version name", ret == 0 && name && strcmp(name, "requests") == 0);
    TEST("pypi version ver", ver && strcmp(ver, "2.31.0") == 0);
    free(name); free(ver);
}

static void test_parse_pypi_extras(void)
{
    const char *args[] = {"package[extra1,extra2]==1.0"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "PyPI", &name, &ver);
    TEST("pypi extras name stripped", ret == 0 && name && strcmp(name, "package") == 0);
    TEST("pypi extras ver", ver && strcmp(ver, "1.0") == 0);
    free(name); free(ver);
}

static void test_parse_pypi_extras_no_version(void)
{
    const char *args[] = {"package[extra]"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args, 1, "PyPI", &name, &ver);
    TEST("pypi extras no ver name", ret == 0 && name && strcmp(name, "package") == 0);
    TEST("pypi extras no ver NULL", ver == NULL);
    free(name); free(ver);
}

/* ─── Edge Cases ────────────────────────────────────────── */

static void test_parse_edge_cases(void)
{
    /* Skip flags */
    const char *args1[] = {"--verbose", "package"};
    char *name = NULL, *ver = NULL;
    int ret = osv_parse_package(args1, 2, "npm", &name, &ver);
    TEST("skip flags before package", ret == 0 && name && strcmp(name, "package") == 0);
    free(name); free(ver);

    /* No args */
    ret = osv_parse_package(NULL, 0, "npm", &name, &ver);
    TEST("no args returns -1", ret == -1);

    /* Unknown ecosystem */
    const char *args2[] = {"pkg"};
    ret = osv_parse_package(args2, 1, "unknown", &name, &ver);
    TEST("unknown ecosystem returns -1", ret == -1);

    /* Only flags, no package */
    const char *args3[] = {"--help"};
    ret = osv_parse_package(args3, 1, "npm", &name, &ver);
    TEST("only flags returns -1", ret == -1);
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    test_infer_ecosystem();
    test_parse_npm_unscoped();
    test_parse_npm_version();
    test_parse_npm_latest();
    test_parse_npm_scoped();
    test_parse_npm_scoped_version();
    test_parse_pypi_simple();
    test_parse_pypi_version();
    test_parse_pypi_extras();
    test_parse_pypi_extras_no_version();
    test_parse_edge_cases();

    printf("osv: %d/%d passed\n", passed, tests);
    return passed == tests ? 0 : 1;
}
