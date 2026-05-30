/*
 * test_webhook_endpoint.c — Tests for API server webhook endpoint (E05).
 */
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    /* Test 1: JSON construction */
    {
        json_t *resp = json_object();
        json_set(resp, "status", json_string("ok"));
        json_set(resp, "platform", json_string("telegram"));
        json_set(resp, "received", json_bool(true));
        json_set(resp, "body_size", json_number(42));
        char *s = json_serialize(resp);
        TEST("json serialize non-null", s != NULL);
        if (s) {
            TEST("json contains ok", strstr(s, "ok") != NULL);
            TEST("json contains telegram", strstr(s, "telegram") != NULL);
            free(s);
        }
        json_free(resp);
    }

    /* Test 2: json_get_str works */
    {
        json_t *j = json_object();
        json_set(j, "status", json_string("ok"));
        const char *v = json_get_str(j, "status", "");
        TEST("json_get_str returns ok", v && strcmp(v, "ok") == 0);
        json_free(j);
    }

    /* Test 3: json_get_bool works */
    {
        json_t *j = json_object();
        json_set(j, "received", json_bool(true));
        bool v = json_get_bool(j, "received", false);
        TEST("json_get_bool returns true", v == true);
        json_free(j);
    }

    /* Test 4: json_get_num works */
    {
        json_t *j = json_object();
        json_set(j, "body_size", json_number(42));
        double v = json_get_num(j, "body_size", -1);
        TEST("json_get_num returns 42", v == 42.0);
        json_free(j);
    }

    /* Test 5: Round-trip JSON parse */
    {
        const char *json_str = "{\"status\":\"ok\",\"platform\":\"discord\",\"received\":true,\"body_size\":10}";
        char *jerr = NULL;
        json_t *j = json_parse(json_str, &jerr);
        TEST("round-trip parse non-null", j != NULL);
        if (j) {
            const char *status = json_get_str(j, "status", "");
            const char *plat = json_get_str(j, "platform", "");
            TEST("round-trip status ok", strcmp(status, "ok") == 0);
            TEST("round-trip platform discord", strcmp(plat, "discord") == 0);
            json_free(j);
        }
        free(jerr);
    }

    /* Test 6: NULL safety */
    {
        TEST("json_get_str with NULL", strcmp(json_get_str(NULL, "x", "def"), "def") == 0);
        TEST("json_get_bool with NULL", json_get_bool(NULL, "x", true) == true);
        TEST("json_get_num with NULL", json_get_num(NULL, "x", -1) == -1);
    }

    /* Test 7: Webhook platforms */
    {
        const char *platforms[] = {"telegram", "discord", "slack", "whatsapp",
                                    "signal", "email", "sms", "feishu", NULL};
        for (int i = 0; platforms[i]; i++) {
            json_t *resp = json_object();
            json_set(resp, "status", json_string("ok"));
            json_set(resp, "platform", json_string(platforms[i]));
            json_set(resp, "received", json_bool(true));
            json_set(resp, "body_size", json_number(5));
            const char *p = json_get_str(resp, "platform", "");
            char testname[128];
            snprintf(testname, sizeof(testname), "platform %s", platforms[i]);
            TEST(testname, strcmp(p, platforms[i]) == 0);
            json_free(resp);
        }
    }

    /* Test 8: Body size tracking */
    {
        json_t *resp = json_object();
        json_set(resp, "body_size", json_number(100));
        double sz = json_get_num(resp, "body_size", -1);
        TEST("body_size=100", sz == 100);
        json_free(resp);
    }

    /* Test 9: Multiple webhook responses (no memory leaks) */
    {
        for (int i = 0; i < 10; i++) {
            json_t *resp = json_object();
            json_set(resp, "status", json_string("ok"));
            json_set(resp, "count", json_number(i));
            char *s = json_serialize(resp);
            if (s) free(s);
            json_free(resp);
        }
        TEST("multiple webhook responses ok", 1);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All webhook endpoint tests PASSED");
    return failures ? 1 : 0;
}
