/* test_video_gen_registry.c — Tests for video_gen_registry.
 * Port of Python tests/agent/test_image_gen_registry.py concepts.
 */
#include "video_gen_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test registration and lookup */
static void test_register_and_lookup(void) {
    video_gen_reset_registry();
    assert(video_gen_provider_count() == 0);

    video_gen_provider_t fal;
    memset(&fal, 0, sizeof(fal));
    snprintf(fal.name, sizeof(fal.name), "fal");
    snprintf(fal.display_name, sizeof(fal.display_name), "FAL.ai");
    fal.is_available = NULL;

    assert(video_gen_register_provider(&fal) == true);
    assert(video_gen_provider_count() == 1);

    const video_gen_provider_t *p = video_gen_get_provider("fal");
    assert(p != NULL);
    assert(strcmp(p->name, "fal") == 0);
    assert(strcmp(p->display_name, "FAL.ai") == 0);

    /* Non-existent provider */
    assert(video_gen_get_provider("nonexistent") == NULL);
}

/* Test re-registration overwrites */
static void test_reregister(void) {
    video_gen_reset_registry();

    video_gen_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "fal");
    snprintf(a.display_name, sizeof(a.display_name), "Old");
    video_gen_register_provider(&a);

    memset(&b, 0, sizeof(b));
    snprintf(b.name, sizeof(b.name), "fal");
    snprintf(b.display_name, sizeof(b.display_name), "New");
    video_gen_register_provider(&b);

    assert(video_gen_provider_count() == 1);
    const video_gen_provider_t *p = video_gen_get_provider("fal");
    assert(p != NULL);
    assert(strcmp(p->display_name, "New") == 0);
}

/* Test empty name rejection */
static void test_empty_name(void) {
    video_gen_reset_registry();

    video_gen_provider_t p;
    memset(&p, 0, sizeof(p));
    assert(video_gen_register_provider(&p) == false);
    assert(video_gen_provider_count() == 0);

    /* Also test NULL provider */
    assert(video_gen_register_provider(NULL) == false);
}

/* Test NULL is_available handling in get_active_provider */
static void test_active_provider_fallback(void) {
    video_gen_reset_registry();

    /* No providers → no active */
    assert(video_gen_get_active_provider() == NULL);

    /* One provider, no is_available check needed (NULL = assume available) */
    video_gen_provider_t fal;
    memset(&fal, 0, sizeof(fal));
    snprintf(fal.name, sizeof(fal.name), "fal");
    snprintf(fal.display_name, sizeof(fal.display_name), "FAL.ai");
    fal.is_available = NULL;
    video_gen_register_provider(&fal);

    /* Single provider fallback should work even with NULL is_available */
    const video_gen_provider_t *active = video_gen_get_active_provider();
    assert(active != NULL);
    assert(strcmp(active->name, "fal") == 0);
}

/* Test index-based access */
static void test_by_index(void) {
    video_gen_reset_registry();

    video_gen_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "alpha");
    video_gen_register_provider(&a);
    snprintf(b.name, sizeof(b.name), "beta");
    video_gen_register_provider(&b);

    assert(video_gen_provider_count() == 2);
    const video_gen_provider_t *p0 = video_gen_get_provider_by_index(0);
    const video_gen_provider_t *p1 = video_gen_get_provider_by_index(1);
    assert(p0 != NULL && p1 != NULL);
    assert(strcmp(p0->name, "alpha") == 0);
    assert(strcmp(p1->name, "beta") == 0);
    assert(video_gen_get_provider_by_index(-1) == NULL);
    assert(video_gen_get_provider_by_index(2) == NULL);
}

/* Test reset */
static void test_reset(void) {
    video_gen_reset_registry();
    assert(video_gen_provider_count() == 0);

    video_gen_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "test");
    video_gen_register_provider(&p);
    assert(video_gen_provider_count() == 1);

    video_gen_reset_registry();
    assert(video_gen_provider_count() == 0);
}

int main(void) {
    test_register_and_lookup();
    test_reregister();
    test_empty_name();
    test_active_provider_fallback();
    test_by_index();
    test_reset();

    printf("video_gen_registry: 6/6 pass\n");
    return 0;
}
