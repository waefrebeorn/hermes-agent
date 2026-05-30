/*
 * test_yuanbao_tools.c — Tests for Yuanbao sticker search and lookup.
 * Includes source directly to access static functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include the source under test */
#include "../src/tools/yuanbao_tools.c"

/* Test counters */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [L%d]: %s\n", __LINE__, msg); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

#define ASSERT_CONTAINS(str, substr, msg) do { \
    if (!strstr(str, substr)) { \
        fprintf(stderr, "  FAIL [L%d]: %s — expected '%s' in result\n", __LINE__, msg, substr); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

int main(void) {
    printf("=== Yuanbao Tools Tests ===\n");

    /* Test 1: No query returns first N stickers */
    {
        char *res = yb_search_sticker_handler("{}", "");
        ASSERT(res != NULL, "search with empty args returns non-NULL");
        ASSERT_CONTAINS(res, "\"success\":true", "empty query returns success");
        ASSERT_CONTAINS(res, "\"count\":10", "empty query returns 10 results (default limit)");
        free(res);
    }

    /* Test 2: Null args returns error */
    {
        char *res = yb_search_sticker_handler(NULL, "");
        ASSERT(res != NULL, "null args returns non-NULL");
        ASSERT_CONTAINS(res, "error", "null args returns error");
        ASSERT_CONTAINS(res, "Failed to parse", "null args returns parse error");
        free(res);
    }

    /* Test 3: Exact sticker ID match "278" returns 六六六 */
    {
        char *res = yb_search_sticker_handler("{\"query\":\"278\"}", "");
        ASSERT(res != NULL, "search '278' returns non-NULL");
        ASSERT_CONTAINS(res, "\"success\":true", "search '278' returns success");
        ASSERT_CONTAINS(res, "\"sticker_id\":\"278\"", "search '278' finds sticker ID 278");
        free(res);
    }

    /* Test 4: Custom limit */
    {
        char *res = yb_search_sticker_handler("{\"limit\":3}", "");
        ASSERT(res != NULL, "limit=3 returns non-NULL");
        ASSERT_CONTAINS(res, "\"count\":3", "limit=3 returns 3 results");
        free(res);
    }

    /* Test 5: Limit cap at 50 */
    {
        char *res = yb_search_sticker_handler("{\"limit\":100}", "");
        ASSERT(res != NULL, "limit=100 returns non-NULL");
        ASSERT_CONTAINS(res, "\"count\":50", "limit=100 capped to 50 results");
        free(res);
    }

    /* Test 6: Sticker database has correct count */
    {
        ASSERT(YB_STICKER_COUNT == 59, "sticker database has 59 entries");
    }

    /* Test 7: Verify first sticker entry */
    {
        ASSERT(strcmp(YB_STICKERS[0].sticker_id, "278") == 0, "first sticker ID is 278");
        ASSERT(strcmp(YB_STICKERS[0].name, "六六六") == 0, "first sticker name is '六六六'");
        ASSERT(YB_STICKERS[0].width == 128, "first sticker width is 128");
        ASSERT(YB_STICKERS[0].height == 128, "first sticker height is 128");
    }

    /* Test 8: Verify specific sticker entries have correct IDs */
    {
        ASSERT(strcmp(YB_STICKERS[1].sticker_id, "262") == 0, "sticker[1] ID is 262");
        ASSERT(strcmp(YB_STICKERS[2].sticker_id, "130") == 0, "sticker[2] ID is 130 (害羞)");
        ASSERT(strcmp(YB_STICKERS[3].sticker_id, "252") == 0, "sticker[3] ID is 252 (比心)");
    }

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
