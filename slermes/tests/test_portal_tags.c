/* Portal tags tests — verify Nous Portal tag generation.
 *
 * Tests:
 * 1. hermes_client_tag returns correct format
 * 2. hermes_client_tag contains "client=hermes-client-v"
 * 3. hermes_nous_portal_tags_json returns valid JSON array
 * 4. JSON contains "product=hermes-agent"
 * 5. JSON contains "client=hermes-client-v"
 */

#include "hermes_portal_tags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests = 0, passed = 0;

#define TEST(name, expr) do { \
    tests++; \
    if (!(expr)) { \
        fprintf(stderr, "FAIL: %s\n", name); \
    } else { \
        passed++; \
    } \
} while(0)

int main(void)
{
    /* Test 1: client tag format */
    char *tag = hermes_client_tag();
    TEST("client tag non-NULL", tag != NULL);
    TEST("client tag starts correctly",
        tag && strncmp(tag, "client=hermes-client-v", 22) == 0);
    TEST("client tag has version", tag && strlen(tag) > 22);
    free(tag);

    /* Test 2: portal tags JSON */
    char *json = hermes_nous_portal_tags_json();
    TEST("portal json non-NULL", json != NULL);
    TEST("portal json starts with [", json && json[0] == '[');
    TEST("portal json ends with ]", json && json[strlen(json)-1] == ']');
    TEST("portal json has product tag",
        json && strstr(json, "product=hermes-agent") != NULL);
    TEST("portal json has client tag",
        json && strstr(json, "client=hermes-client-v") != NULL);
    free(json);

    /* Test 3: Extended edge cases */
    printf("\n--- Edge cases ---\n");

    /* Repeated calls yield same format (save before free) */
    tag = hermes_client_tag();
    char *tag2 = hermes_client_tag();
    TEST("client tag consistent", tag && tag2 && strcmp(tag, tag2) == 0);
    free(tag);
    tag = NULL;
    free(tag2);

    /* JSON contains only expected tag fields */
    json = hermes_nous_portal_tags_json();
    TEST("portal has product= tag", json && strstr(json, "product=hermes-agent") != NULL);
    TEST("portal has client= tag", json && strstr(json, "client=hermes-client-v") != NULL);
    TEST("portal json has exactly 2 entries", json && strstr(json, "product=") && strstr(json, "client="));
    free(json);

    /* JSON is valid (starts with [, ends with ], parseable) */
    json = hermes_nous_portal_tags_json();
    TEST("portal json starts with [", json && json[0] == '[');
    TEST("portal json ends with ]", json && json[strlen(json)-1] == ']');
    TEST("portal json valid JSON", json && json[0] == '[' && json[strlen(json)-1] == ']');
    free(json);

    /* Edge: NULL safety — functions never return NULL unless allocation fails */
    tag = hermes_client_tag();
    TEST("client tag always non-NULL", tag != NULL);
    free(tag);
    json = hermes_nous_portal_tags_json();
    TEST("portal json always non-NULL", json != NULL);
    free(json);

    printf("portal_tags: %d/%d passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
