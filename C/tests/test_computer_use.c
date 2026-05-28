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

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
