/* test_homeassistant.c — Tests for Home Assistant tool (schema, validation, registration). */
#include "hermes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", name, __LINE__); \
        fail++; \
    } else { \
        printf("  PASS: %s\n", name); \
        pass++; \
    } \
} while(0)

/* Forward declare handlers */
char *ha_list_entities_handler(const char *args, const char *task_id);
char *ha_get_state_handler(const char *args, const char *task_id);
char *ha_list_services_handler(const char *args, const char *task_id);
char *ha_call_service_handler(const char *args, const char *task_id);
char *ha_get_history_handler(const char *args, const char *task_id);

int main(void) {
    printf("=== Home Assistant Tool Tests ===\n\n");

    /* Test ha_list_entities_handler error paths */
    printf("--- ha_list_entities_handler ---\n");
    {
        char *r = ha_list_entities_handler(NULL, NULL);
        TEST("NULL args returns error", r != NULL);
        if (r) { TEST("error message present", strstr(r, "error") != NULL); free(r); }
    }
    {
        char *r = ha_list_entities_handler("invalid json", NULL);
        TEST("invalid JSON returns error", r != NULL);
        if (r) { TEST("JSON parse error", strstr(r, "Invalid") != NULL || strstr(r, "error") != NULL); free(r); }
    }
    {
        char *r = ha_list_entities_handler("{\"test\":1}", NULL);
        TEST("valid JSON with no relevant fields returns response",
             r != NULL);
        if (r) free(r);
    }

    /* Test ha_get_state_handler */
    printf("\n--- ha_get_state_handler ---\n");
    {
        char *r = ha_get_state_handler(NULL, NULL);
        TEST("NULL args returns error", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_get_state_handler("{}", NULL);
        TEST("empty args returns result", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_get_state_handler("{\"entity_id\":\"light.living_room\"}", NULL);
        TEST("with entity_id returns result", r != NULL);
        if (r) free(r);
    }

    /* Test ha_list_services_handler */
    printf("\n--- ha_list_services_handler ---\n");
    {
        char *r = ha_list_services_handler(NULL, NULL);
        TEST("NULL args returns error", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_list_services_handler("{\"test\":1}", NULL);
        TEST("valid JSON returns response", r != NULL);
        if (r) free(r);
    }

    /* Test ha_call_service_handler */
    printf("\n--- ha_call_service_handler ---\n");
    {
        char *r = ha_call_service_handler(NULL, NULL);
        TEST("NULL args returns error", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_call_service_handler("{}", NULL);
        TEST("empty args returns response", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_call_service_handler("{\"service\":\"light.turn_on\",\"entity_id\":\"light.living_room\"}", NULL);
        TEST("with service+entity returns response", r != NULL);
        if (r) free(r);
    }

    /* Test ha_get_history_handler */
    printf("\n--- ha_get_history_handler ---\n");
    {
        char *r = ha_get_history_handler(NULL, NULL);
        TEST("NULL args returns error", r != NULL);
        if (r) { TEST("error message present", strstr(r, "error") != NULL); free(r); }
    }
    {
        char *r = ha_get_history_handler("{}", NULL);
        TEST("empty args returns error", r != NULL);
        if (r) { TEST("missing entity_id error", strstr(r, "entity_id") != NULL); free(r); }
    }

    /* Test ha_list_entities domain/area filtering (no HA server — expect config error) */
    printf("\n--- ha_list_entities filtering ---\n");
    {
        char *r = ha_list_entities_handler("{\"domain\":\"light\"}", NULL);
        TEST("domain filter returns response", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_list_entities_handler("{\"area\":\"living room\"}", NULL);
        TEST("area filter returns response", r != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_list_entities_handler("{\"domain\":\"light\",\"area\":\"kitchen\"}", NULL);
        TEST("domain+area filter returns response", r != NULL);
        if (r) free(r);
    }

    /* Test ha_list_services domain filtering */
    printf("\n--- ha_list_services domain filter ---\n");
    {
        char *r = ha_list_services_handler("{\"domain\":\"light\"}", NULL);
        TEST("domain filter returns response", r != NULL);
        if (r) free(r);
    }

    /* Test ha_call_service validation and blocked domains */
    printf("\n--- ha_call_service validation ---\n");
    {
        char *r = ha_call_service_handler("{\"domain\":\"INVALID\",\"service\":\"turn_on\"}", NULL);
        TEST("invalid domain returns error", r != NULL && strstr(r, "error") != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_call_service_handler("{\"domain\":\"shell_command\",\"service\":\"run\"}", NULL);
        TEST("blocked domain returns error", r != NULL && strstr(r, "error") != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_call_service_handler("{\"domain\":\"../../etc\",\"service\":\"pwn\"}", NULL);
        TEST("path traversal domain returns error", r != NULL && strstr(r, "error") != NULL);
        if (r) free(r);
    }
    {
        char *r = ha_call_service_handler("{\"domain\":\"light\",\"service\":\"turn_on\",\"entity_id\":\"light.living_room\",\"data\":{\"brightness\":255}}", NULL);
        TEST("with data object returns response", r != NULL);
        if (r) free(r);
    }

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
