/*
 * test_telegram_fallback_ips.c — Tests for Telegram fallback IP parsing.
 * Standalone — inline logic from telegram.c for portability.
 * Compile: gcc -O2 -Wall -Wextra -o /tmp/t_tg_fb test_telegram_fallback_ips.c -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

/* Inline port of telegram_parse_fallback_ips() for testing */
static char **parse_fallback_ips(const char *env_value, size_t *count) {
    if (!env_value || !env_value[0] || !count) {
        if (count) *count = 0;
        return NULL;
    }
    size_t capacity = 32;
    size_t n = 0;
    char **result = (char **)calloc(capacity, sizeof(char *));
    if (!result) { *count = 0; return NULL; }

    char *dup = strdup(env_value);
    if (!dup) { free(result); *count = 0; return NULL; }
    char *saveptr = NULL;
    for (char *tok = strtok_r(dup, ",", &saveptr); tok; tok = strtok_r(NULL, ",", &saveptr)) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        if (!*tok) continue;

        struct in_addr addr;
        if (inet_pton(AF_INET, tok, &addr) != 1) continue;
        const unsigned char *b = (const unsigned char *)&addr;
        if ((b[0] == 10) ||
            (b[0] == 172 && (b[1] >= 16 && b[1] <= 31)) ||
            (b[0] == 192 && b[1] == 168)) continue;
        if (b[0] == 127) continue;
        if (b[0] == 169 && b[1] == 254) continue;
        unsigned int ip32 = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        if (ip32 == 0) continue;

        if (n >= capacity) {
            capacity *= 2;
            char **nr = (char **)realloc(result, capacity * sizeof(char *));
            if (!nr) break;
            result = nr;
        }
        result[n] = strdup(tok);
        if (result[n]) n++;
    }
    free(dup);
    *count = n;
    return result;
}

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

static void free_ips(char **ips, size_t count) {
    for (size_t i = 0; i < count; i++) free(ips[i]);
    free(ips);
}

int main(void) {
    printf("=== Telegram Fallback IP Tests ===\n");

    /* Test 1: NULL env value returns NULL/0 */
    {
        size_t count = 99;
        char **ips = parse_fallback_ips(NULL, &count);
        TEST("NULL input returns NULL", ips == NULL);
        TEST("NULL input sets count=0", count == 0);
    }

    /* Test 2: empty env value returns NULL/0 */
    {
        size_t count = 99;
        char **ips = parse_fallback_ips("", &count);
        TEST("empty input returns NULL", ips == NULL);
        TEST("empty input sets count=0", count == 0);
    }

    /* Test 3: valid public IPv4 address */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("149.154.167.91", &count);
        TEST("valid public IP accepted", ips != NULL && count == 1);
        TEST("valid public IP correct", ips && count > 0 && strcmp(ips[0], "149.154.167.91") == 0);
        free_ips(ips, count);
    }

    /* Test 4: private IP (10.x.x.x) rejected */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("10.0.0.1", &count);
        TEST("private 10.x rejects", ips == NULL || count == 0);
        free_ips(ips, count);
    }

    /* Test 5: loopback (127.0.0.1) rejected */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("127.0.0.1", &count);
        TEST("loopback 127.x rejects", ips == NULL || count == 0);
        free_ips(ips, count);
    }

    /* Test 6: link-local (169.254.x.x) rejected */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("169.254.1.1", &count);
        TEST("link-local 169.254.x rejects", ips == NULL || count == 0);
        free_ips(ips, count);
    }

    /* Test 7: invalid IP string rejected */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("not_an_ip", &count);
        TEST("invalid IP rejects", ips == NULL || count == 0);
        free_ips(ips, count);
    }

    /* Test 8: comma-separated multiple IPs */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("149.154.167.91,149.154.167.92", &count);
        TEST("two valid IPs accepted", ips != NULL && count == 2);
        if (ips && count >= 2) {
            TEST("first IP correct", strcmp(ips[0], "149.154.167.91") == 0);
            TEST("second IP correct", strcmp(ips[1], "149.154.167.92") == 0);
        }
        free_ips(ips, count);
    }

    /* Test 9: mixed valid/invalid — only valid kept */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("149.154.167.91,192.168.1.1,not_ip", &count);
        TEST("mixed input keeps only valid", ips != NULL && count == 1);
        if (ips && count > 0) {
            TEST("mixed keeps public IP", strcmp(ips[0], "149.154.167.91") == 0);
        }
        free_ips(ips, count);
    }

    /* Test 10: IPv6 rejected */
    {
        size_t count = 0;
        char **ips = parse_fallback_ips("::1", &count);
        TEST("IPv6 rejects", ips == NULL || count == 0);
        free_ips(ips, count);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All fallback IP tests PASSED");
    return failures ? 1 : 0;
}
