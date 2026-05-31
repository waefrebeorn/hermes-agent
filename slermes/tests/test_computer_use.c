/*
 * test_computer_use.c — Computer use tool unit tests.
 *
 * MIT License — WuBu Hermes Project
 */

#include "hermes_computer_use.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int pass = 0, fail = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); \
        pass++; \
    } else { \
        printf("FAIL\n"); \
        fail++; \
    } \
} while(0)

/* ── Helper: fresh noop backend ── */
static cu_backend_t *make_noop(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (b) b->start();
    return b;
}

static void destroy_noop(cu_backend_t *b) {
    if (b) { b->stop(); free(b->state); free(b); }
}

/* ── Test: noop backend lifecycle ── */
static int test_noop_lifecycle(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b) return 0;
    int ok = 1;
    ok = ok && b->is_available();
    ok = ok && b->start();
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop capture ── */
static int test_noop_capture(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_capture_t *cap = b->capture("som", NULL);
    int ok = (cap != NULL && cap->width == 1024 && cap->height == 768
              && strcmp(cap->mode, "som") == 0);
    cu_capture_free(cap);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop click ── */
static int test_noop_click(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->click(1, 100, 200, "left", 1, NULL);
    int ok = (act != NULL && act->ok && strcmp(act->action, "click") == 0);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop type ── */
static int test_noop_type(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->type_text("hello world");
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop wait ── */
static int test_noop_wait(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->wait(0.01);
    int ok = (act != NULL && act->ok && strcmp(act->action, "wait") == 0);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop key ── */
static int test_noop_key(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->key("cmd+s");
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop list_apps ── */
static int test_noop_list_apps(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->list_apps();
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: noop focus_app ── */
static int test_noop_focus(void) {
    cu_backend_t *b = computer_use_new_noop_backend();
    if (!b || !b->start()) { free(b); return 0; }
    cu_action_t *act = b->focus_app("Safari", false);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    b->stop();
    free(b->state);
    free(b);
    return ok;
}

/* ── Test: global backend selection ── */
static int test_global_backend(void) {
    cu_reset_global_backend();
    cu_backend_t *b = cu_get_global_backend();
    int ok = (b != NULL && b->is_available());
    cu_reset_global_backend();
    return ok;
}

/* ── Integration test: full lifecycle via public API ── */
static int test_full_lifecycle(void) {
    cu_reset_global_backend();
    cu_backend_t *b = cu_get_global_backend();
    int ok = (b != NULL);

    cu_capture_t *cap = b->capture("som", "Test");
    ok = ok && (cap != NULL);
    if (cap) {
        ok = ok && (cap->width > 0);
        cu_capture_free(cap);
    }

    cu_action_t *act = b->click(0, 100, 200, "left", 1, NULL);
    ok = ok && (act != NULL && act->ok);
    cu_action_free(act);

    act = b->type_text("hello");
    ok = ok && (act != NULL && act->ok);
    cu_action_free(act);

    act = b->wait(0.01);
    ok = ok && (act != NULL && act->ok);
    cu_action_free(act);

    cu_reset_global_backend();
    return ok;
}

/* ── Edge case: NULL free safety ── */
static int test_null_free(void) {
    cu_capture_free(NULL);
    cu_action_free(NULL);
    return 1;
}

/* ── Edge case: click with NULL button ── */
static int test_click_null_button(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->click(0, 100, 200, NULL, 1, NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: type with NULL text ── */
static int test_type_null_text(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->type_text(NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: type with empty text ── */
static int test_type_empty_text(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->type_text("");
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: type with very long text ── */
static int test_type_long_text(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    char long_text[4001];
    memset(long_text, 'A', sizeof(long_text) - 1);
    long_text[sizeof(long_text) - 1] = '\0';
    cu_action_t *act = b->type_text(long_text);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: key with NULL keys ── */
static int test_key_null(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->key(NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: key with empty keys ── */
static int test_key_empty(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->key("");
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: focus with NULL app ── */
static int test_focus_null_app(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->focus_app(NULL, false);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: focus with empty app ── */
static int test_focus_empty_app(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->focus_app("", false);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: scroll with NULL direction ── */
static int test_scroll_null_dir(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->scroll(NULL, 0, 0, 0, 0, NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: scroll with zero amount ── */
static int test_scroll_zero_amount(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->scroll("down", 0, 0, 0, 0, NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: wait with zero seconds ── */
static int test_wait_zero(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->wait(0.0);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: wait with negative seconds ── */
static int test_wait_negative(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->wait(-0.5);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: set_value with NULL value ── */
static int test_set_value_null(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->set_value(NULL, 0);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: set_value with negative element ── */
static int test_set_value_negative(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->set_value("hello", -1);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: drag with negative elements ── */
static int test_drag_negative(void) {
    cu_backend_t *b = make_noop();
    if (!b) return 0;
    cu_action_t *act = b->drag(-1, -1, 0, 0, 100, 100, "left", NULL);
    int ok = (act != NULL && act->ok);
    cu_action_free(act);
    destroy_noop(b);
    return ok;
}

/* ── Edge case: backend registry ── */
static int test_backend_registry(void) {
    cu_clear_backends();
    int ok = 1;
    ok = ok && cu_register_backend("test_1", "Test backend", computer_use_new_noop_backend);
    ok = ok && cu_register_backend("test_2", "Another test", computer_use_new_noop_backend);
    ok = ok && cu_register_backend("test_1", "Duplicate name", computer_use_new_noop_backend); /* allowed, overwrites */
    /* Check list */
    char *list = cu_list_backends();
    ok = ok && (list != NULL);
    if (list) {
        ok = ok && (strstr(list, "test_1") != NULL);
        ok = ok && (strstr(list, "test_2") != NULL);
        free(list);
    }
    cu_clear_backends();
    list = cu_list_backends();
    ok = ok && (list != NULL && strstr(list, "test_1") == NULL);
    free(list);
    return ok;
}

int main(void) {
    printf("=== Computer Use Tests (S01-S02) ===\n");
    TEST(noop_lifecycle);
    TEST(noop_capture);
    TEST(noop_click);
    TEST(noop_type);
    TEST(noop_wait);
    TEST(noop_key);
    TEST(noop_list_apps);
    TEST(noop_focus);
    TEST(global_backend);
    TEST(full_lifecycle);

    printf("\n--- Edge Cases ---\n");
    TEST(null_free);
    TEST(click_null_button);
    TEST(type_null_text);
    TEST(type_empty_text);
    TEST(type_long_text);
    TEST(key_null);
    TEST(key_empty);
    TEST(focus_null_app);
    TEST(focus_empty_app);
    TEST(scroll_null_dir);
    TEST(scroll_zero_amount);
    TEST(wait_zero);
    TEST(wait_negative);
    TEST(set_value_null);
    TEST(set_value_negative);
    TEST(drag_negative);
    TEST(backend_registry);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
