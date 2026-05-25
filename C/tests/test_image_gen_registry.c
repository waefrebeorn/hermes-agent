/* test_image_gen_registry.c — Tests for image_gen_registry.
 * Port of Python tests/agent/test_image_gen_registry.py concepts.
 */
#include "image_gen_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test registration and lookup */
static void test_register_and_lookup(void) {
    image_gen_reset_registry();
    assert(image_gen_provider_count() == 0);

    image_gen_provider_t fal;
    memset(&fal, 0, sizeof(fal));
    snprintf(fal.name, sizeof(fal.name), "fal");
    snprintf(fal.display_name, sizeof(fal.display_name), "FAL.ai");
    fal.is_available = NULL;

    assert(image_gen_register_provider(&fal) == true);
    assert(image_gen_provider_count() == 1);

    const image_gen_provider_t *p = image_gen_get_provider("fal");
    assert(p != NULL);
    assert(strcmp(p->name, "fal") == 0);
    assert(strcmp(p->display_name, "FAL.ai") == 0);

    assert(image_gen_get_provider("nonexistent") == NULL);
}

/* Test re-registration overwrites */
static void test_reregister(void) {
    image_gen_reset_registry();

    image_gen_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "fal");
    snprintf(a.display_name, sizeof(a.display_name), "Old");
    image_gen_register_provider(&a);

    memset(&b, 0, sizeof(b));
    snprintf(b.name, sizeof(b.name), "fal");
    snprintf(b.display_name, sizeof(b.display_name), "New");
    image_gen_register_provider(&b);

    assert(image_gen_provider_count() == 1);
    const image_gen_provider_t *p = image_gen_get_provider("fal");
    assert(p != NULL);
    assert(strcmp(p->display_name, "New") == 0);
}

/* Test empty name rejection */
static void test_empty_name(void) {
    image_gen_reset_registry();

    image_gen_provider_t p;
    memset(&p, 0, sizeof(p));
    assert(image_gen_register_provider(&p) == false);
    assert(image_gen_provider_count() == 0);

    assert(image_gen_register_provider(NULL) == false);
}

/* Test NULL is_available handling */
static void test_active_fallback_single(void) {
    image_gen_reset_registry();
    assert(image_gen_get_active_provider() == NULL);

    image_gen_provider_t fal;
    memset(&fal, 0, sizeof(fal));
    snprintf(fal.name, sizeof(fal.name), "fal");
    fal.is_available = NULL;
    image_gen_register_provider(&fal);

    const image_gen_provider_t *active = image_gen_get_active_provider();
    assert(active != NULL);
    assert(strcmp(active->name, "fal") == 0);
}

/* Test index-based access */
static void test_by_index(void) {
    image_gen_reset_registry();

    image_gen_provider_t a, b;
    memset(&a, 0, sizeof(a));
    snprintf(a.name, sizeof(a.name), "alpha");
    image_gen_register_provider(&a);
    snprintf(b.name, sizeof(b.name), "beta");
    image_gen_register_provider(&b);

    assert(image_gen_provider_count() == 2);
    assert(image_gen_get_provider_by_index(0) != NULL);
    assert(image_gen_get_provider_by_index(1) != NULL);
    assert(strcmp(image_gen_get_provider_by_index(0)->name, "alpha") == 0);
    assert(strcmp(image_gen_get_provider_by_index(1)->name, "beta") == 0);
    assert(image_gen_get_provider_by_index(-1) == NULL);
    assert(image_gen_get_provider_by_index(2) == NULL);
}

/* Test reset */
static void test_reset(void) {
    image_gen_reset_registry();
    assert(image_gen_provider_count() == 0);

    image_gen_provider_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.name, sizeof(p.name), "test");
    image_gen_register_provider(&p);
    assert(image_gen_provider_count() == 1);

    image_gen_reset_registry();
    assert(image_gen_provider_count() == 0);
}

int main(void) {
    test_register_and_lookup();
    test_reregister();
    test_empty_name();
    test_active_fallback_single();
    test_by_index();
    test_reset();

    printf("image_gen_registry: 6/6 pass\n");
    return 0;
}
