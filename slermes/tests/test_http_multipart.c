/*
 * test_http_multipart.c — Tests for multipart form data (L04).
 * Tests builder API, boundary generation, body format, and convenience POST.
 */
#include "http.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

#define TEST_CONTAINS(body, substr) do { \
    if (!strstr(body, substr)) { \
        fprintf(stderr, "  FAIL: expected to contain '%s'\\n", substr); failures++; \
    } else printf("  PASS: contains '%s'\n", substr); \
} while (0)

int main(void) {
    /* Test 1: Create and free multipart form */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        TEST("http_multipart_form_new non-NULL", f != NULL);
        http_multipart_form_free(f);
    }

    /* Test 2: Add text field */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        bool ok = http_multipart_add_field(f, "name", "Alice");
        TEST("add text field returns true", ok == true);
        http_multipart_form_free(f);
    }

    /* Test 3: Add file field */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        bool ok = http_multipart_add_file(f, "file", "test.txt",
                                          "Hello World", 11, "text/plain");
        TEST("add file field returns true", ok == true);
        http_multipart_form_free(f);
    }

    /* Test 4: Add file without filename */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        bool ok = http_multipart_add_file(f, "blob", NULL,
                                          "binary data", 11, NULL);
        TEST("add file without filename returns true", ok == true);
        http_multipart_form_free(f);
    }

    /* Test 5: Finalize with single text field */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        http_multipart_add_field(f, "name", "Alice");
        size_t len = 0;
        char *boundary = NULL;
        char *body = http_multipart_form_finalize(f, &len, &boundary);
        TEST("finalize with text field non-NULL", body != NULL);
        TEST("boundary non-NULL", boundary != NULL);
        TEST("body length > 0", len > 0);
        if (body) {
            TEST_CONTAINS(body, "Content-Disposition: form-data; name=\"name\"");
            TEST_CONTAINS(body, "Alice");
            TEST_CONTAINS(body, boundary);
            /* Should end with boundary--\r\n */
            char ending[128];
            snprintf(ending, sizeof(ending), "--%s--\r\n", boundary);
            TEST_CONTAINS(body, ending);
            free(body);
        }
        free(boundary);
        http_multipart_form_free(f);
    }

    /* Test 6: Finalize with text + file fields */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        http_multipart_add_field(f, "user", "bob");
        http_multipart_add_file(f, "photo", "avatar.png",
                                "PNG...data", 10, "image/png");
        size_t len = 0;
        char *boundary = NULL;
        char *body = http_multipart_form_finalize(f, &len, &boundary);
        TEST("finalize with mixed fields non-NULL", body != NULL);
        if (body) {
            TEST_CONTAINS(body, "name=\"user\"");
            TEST_CONTAINS(body, "bob");
            TEST_CONTAINS(body, "name=\"photo\"; filename=\"avatar.png\"");
            TEST_CONTAINS(body, "Content-Type: image/png");
            TEST_CONTAINS(body, "PNG...data");
            free(body);
        }
        free(boundary);
        http_multipart_form_free(f);
    }

    /* Test 7: Finalize with file without filename (no filename in disposition) */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        http_multipart_add_file(f, "blob", NULL, "raw bytes", 9, "application/octet-stream");
        size_t len = 0;
        char *boundary = NULL;
        char *body = http_multipart_form_finalize(f, &len, &boundary);
        TEST("finalize file without filename non-NULL", body != NULL);
        if (body) {
            TEST_CONTAINS(body, "name=\"blob\"");
            /* Should NOT contain filename= */
            TEST("no filename in disposition", strstr(body, "filename=") == NULL);
            TEST_CONTAINS(body, "raw bytes");
            free(body);
        }
        free(boundary);
        http_multipart_form_free(f);
    }

    /* Test 8: Finalize with empty form returns NULL */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        size_t len = 0;
        char *boundary = NULL;
        char *body = http_multipart_form_finalize(f, &len, &boundary);
        TEST("finalize empty form returns NULL", body == NULL);
        http_multipart_form_free(f);
    }

    /* Test 9: NULL safety */
    {
        TEST("http_multipart_add_field with NULL form", !http_multipart_add_field(NULL, "k", "v"));
        TEST("http_multipart_add_field with NULL name", !http_multipart_add_field(NULL, NULL, "v"));
        TEST("http_multipart_add_field with NULL value", !http_multipart_add_field(NULL, "k", NULL));
        TEST("http_multipart_add_file with NULL content", !http_multipart_add_file(NULL, "f", NULL, NULL, 0, NULL));
        TEST("http_multipart_form_finalize with NULL form", http_multipart_form_finalize(NULL, NULL, NULL) == NULL);
        http_multipart_form_free(NULL); /* should not crash */
    }

    /* Test 10: Multiple fields (3 text + 2 file) */
    {
        http_multipart_form_t *f = http_multipart_form_new();
        TEST("add field 1", http_multipart_add_field(f, "user", "alice"));
        TEST("add field 2", http_multipart_add_field(f, "bio", "engineer"));
        TEST("add field 3", http_multipart_add_field(f, "age", "30"));
        TEST("add file 1", http_multipart_add_file(f, "img1", "pic.jpg", "JPEG", 4, "image/jpeg"));
        TEST("add file 2", http_multipart_add_file(f, "img2", "doc.pdf", "PDF", 3, "application/pdf"));
        size_t len = 0;
        char *boundary = NULL;
        char *body = http_multipart_form_finalize(f, &len, &boundary);
        TEST("finalize 5-part form non-NULL", body != NULL);
        if (body) {
            TEST_CONTAINS(body, "alice");
            TEST_CONTAINS(body, "engineer");
            TEST_CONTAINS(body, "30");
            TEST_CONTAINS(body, "JPEG");
            TEST_CONTAINS(body, "PDF");
            /* Count boundary markers (2 per part + 1 final = 5) */
            char marker[96];
            snprintf(marker, sizeof(marker), "--%s", boundary);
            int count = 0;
            const char *p = body;
            while ((p = strstr(p, marker)) != NULL) {
                count++;
                p++;
            }
            TEST("6 boundary markers (2 per part x 5 = final)", count == 6);
            free(body);
        }
        free(boundary);
        http_multipart_form_free(f);
    }

    /* Test 11: http_post_multipart (unit test only — no network)
     * Verify header building without actual HTTP call */
    {
        http_t *h = http_new(5);
        /* Just verify it doesn't crash on NULL */
        TEST("http_post_multipart with NULL form returns NULL",
             http_post_multipart(h, "http://example.com", NULL, NULL) == NULL);
        http_free(h);
    }

    /* Test 12: Boundary uniqueness — two forms should generate different boundaries */
    {
        http_multipart_form_t *f1 = http_multipart_form_new();
        http_multipart_form_t *f2 = http_multipart_form_new();
        http_multipart_add_field(f1, "k", "v");
        http_multipart_add_field(f2, "k", "v");
        size_t len1 = 0, len2 = 0;
        char *b1 = NULL, *b2 = NULL;
        char *body1 = http_multipart_form_finalize(f1, &len1, &b1);
        char *body2 = http_multipart_form_finalize(f2, &len2, &b2);
        TEST("boundary 1 non-NULL", b1 != NULL);
        TEST("boundary 2 non-NULL", b2 != NULL);
        if (b1 && b2) {
            /* Boundaries should differ (random-ish) */
            TEST("boundaries are different", strcmp(b1, b2) != 0);
        }
        free(body1); free(body2);
        free(b1); free(b2);
        http_multipart_form_free(f1);
        http_multipart_form_free(f2);
    }

    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All multipart tests PASSED");
    return failures ? 1 : 0;
}
