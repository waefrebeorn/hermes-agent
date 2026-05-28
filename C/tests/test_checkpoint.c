/*
 * test_checkpoint.c — Tests for checkpoint manager (P98).
 *
 * Build:
 *   gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin \
 *       test_checkpoint.c src/agent/checkpoint.c \
 *       lib/libjson/json.c -o /tmp/test_checkpoint -lm
 *
 * Run:
 *   /tmp/test_checkpoint
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes.h"

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        printf("  \xe2\x9c\x93 %s\n", name); \
        passed++; \
    } else { \
        printf("  \xe2\x9c\x97 %s (FAILED)\n", name); \
        failed++; \
    } \
} while(0)

/* Minimal agent_state with enough fields for checkpoint to work */
static agent_state_t *make_state(void) {
    agent_state_t *s = (agent_state_t *)calloc(1, sizeof(agent_state_t));
    if (!s) return NULL;
    s->message_capacity = 16;
    s->messages = (message_t **)calloc(s->message_capacity, sizeof(message_t *));
    return s;
}

static void free_state(agent_state_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->message_count; i++)
        message_free(s->messages[i]);
    free(s->messages);
    free(s);
}

int main(void) {
    printf("=== Checkpoint Manager Tests (P98) ===\n\n");

    /* --- Test 1: Init --- */
    printf("[P98] Init:\n");
    checkpoint_manager_t mgr;
    checkpoint_init(&mgr);
    TEST("manager initialized", 1);
    TEST("zero checkpoints", mgr.count == 0);
    TEST("default max_snapshots 10", mgr.max_snapshots == 10);
    TEST("default auto_save_interval 5", mgr.auto_save_interval == 5);
    TEST("turn_counter starts 0", mgr.turn_counter == 0);

    /* --- Test 2: Save checkpoint --- */
    printf("\n[P98] Save checkpoint:\n");

    agent_state_t *state = make_state();
    TEST("state created", state != NULL);

    /* Add a test message */
    state->message_count = 1;
    state->messages[0] = (message_t *)calloc(1, sizeof(message_t));
    state->messages[0]->content = strdup("Hello world");
    state->messages[0]->role = MSG_USER;

    bool saved = checkpoint_save(&mgr, state, "test-checkpoint");
    TEST("checkpoint saved", saved);
    TEST("checkpoint count is 1", mgr.count == 1);

    /* Check checkpoint metadata */
    TEST("checkpoint id set", mgr.checkpoints[0].id[0] != '\0');
    TEST("checkpoint label matches", strcmp(mgr.checkpoints[0].label, "test-checkpoint") == 0);
    TEST("checkpoint has messages", mgr.checkpoints[0].count == 1);
    TEST("checkpoint created_at set", mgr.checkpoints[0].created_at > 0);

    /* --- Test 3: Auto-save --- */
    printf("\n[P98] Auto-save:\n");

    /* Need at least 2 messages for autosave to trigger */
    agent_state_t *as_state = make_state();
    as_state->message_count = 5;
    for (size_t i = 0; i < as_state->message_count; i++) {
        as_state->messages[i] = (message_t *)calloc(1, sizeof(message_t));
        as_state->messages[i]->content = strdup("msg");
        as_state->messages[i]->role = MSG_USER;
    }

    checkpoint_manager_t mgr2;
    checkpoint_init(&mgr2);
    mgr2.auto_save_interval = 3;
    mgr2.turn_counter = 0;

    /* First call: turn_counter incremented to 1, < 3 → no save */
    TEST("auto-save skipped turn 1",
         !checkpoint_try_autosave(&mgr2, as_state));
    TEST("counter now 1", mgr2.turn_counter == 1);

    /* Second call: counter → 2 */
    TEST("auto-save skipped turn 2",
         !checkpoint_try_autosave(&mgr2, as_state));
    TEST("counter now 2", mgr2.turn_counter == 2);

    /* Third call: counter → 3, 3 >= 3 → save, counter reset */
    TEST("auto-save at turn 3",
         checkpoint_try_autosave(&mgr2, as_state));
    TEST("counter reset to 0", mgr2.turn_counter == 0);
    TEST("count incremented", mgr2.count == 1);
    free_state(as_state);

    /* --- Test 4: Evict oldest --- */
    printf("\n[P98] Eviction:\n");

    checkpoint_manager_t mgr3;
    checkpoint_init(&mgr3);
    mgr3.max_snapshots = 3;

    /* Save 3 checkpoints */
    TEST("save #1", checkpoint_save(&mgr3, state, "first"));
    TEST("save #2", checkpoint_save(&mgr3, state, "second"));
    TEST("save #3", checkpoint_save(&mgr3, state, "third"));
    TEST("3 checkpoints", mgr3.count == 3);

    /* 4th save should evict oldest */
    TEST("save #4 evicts oldest", checkpoint_save(&mgr3, state, "fourth"));
    TEST("still max 3", mgr3.count == 3);
    TEST("oldest evicted (first gone)", strcmp(mgr3.checkpoints[0].label, "second") == 0);
    TEST("newest is fourth", strcmp(mgr3.checkpoints[2].label, "fourth") == 0);

    /* --- Test 5: List checkpoints --- */
    printf("\n[P98] Listing:\n");

    char ids[4][64] = {{0}};
    char labels[4][128] = {{0}};
    size_t n = checkpoint_list(&mgr3, ids, labels, 4);
    TEST("list returns count > 0", n > 0);
    TEST("list contains 'second'", n > 0 && strstr(labels[0], "second") != NULL);
    TEST("list contains 'fourth'", n > 0 && strstr(labels[n-1], "fourth") != NULL);

    /* --- Test 6: Restore checkpoint --- */
    printf("\n[P98] Restore:\n");

    /* Save state, then corrupt it, then restore */
    agent_state_t *state2 = make_state();
    state2->message_count = 2;
    state2->messages[0] = (message_t *)calloc(1, sizeof(message_t));
    state2->messages[0]->content = strdup("msg1");
    state2->messages[0]->role = MSG_USER;
    state2->messages[1] = (message_t *)calloc(1, sizeof(message_t));
    state2->messages[1]->content = strdup("msg2");
    state2->messages[1]->role = MSG_ASSISTANT;

    /* Save and capture the auto-generated ID */
    checkpoint_manager_t mgr_save;
    checkpoint_init(&mgr_save);
    checkpoint_save(&mgr_save, state2, "pre-change");
    TEST("checkpoint saved", mgr_save.count == 1);
    char saved_id[64];
    snprintf(saved_id, sizeof(saved_id), "%s", mgr_save.checkpoints[0].id);
    TEST("id non-empty", saved_id[0] != '\0');

    /* Corrupt state2 messages */
    for (size_t i = 0; i < state2->message_count; i++) {
        message_free(state2->messages[i]);
        state2->messages[i] = NULL;
    }
    state2->message_count = 0;

    /* Restore by ID */
    bool restored = checkpoint_restore(&mgr_save, state2, saved_id);
    TEST("checkpoint restored by ID", restored);
    TEST("messages restored", state2->message_count == 2);
    TEST("content restored",
         strcmp(state2->messages[0]->content, "msg1") == 0);

    /* Restore most recent (no ID = last checkpoint) */
    agent_state_t *state3 = make_state();
    state3->message_count = 1;
    state3->messages[0] = (message_t *)calloc(1, sizeof(message_t));
    state3->messages[0]->content = strdup("recent-msg");
    state3->messages[0]->role = MSG_USER;
    checkpoint_save(&mgr_save, state3, "recent");
    for (size_t i = 0; i < state3->message_count; i++) {
        message_free(state3->messages[i]);
        state3->messages[i] = NULL;
    }
    state3->message_count = 0;

    restored = checkpoint_restore(&mgr_save, state3, NULL);
    TEST("restore most recent (NULL id)", restored);
    TEST("most recent restored",
         strcmp(state3->messages[0]->content, "recent-msg") == 0);
    free_state(state3);

    free_state(state2);

    /* --- Test 7: Set limits --- */
    printf("\n[P98] Limits:\n");

    checkpoint_manager_t mgr4;
    checkpoint_init(&mgr4);
    checkpoint_set_limits(&mgr4, 20, 10);
    TEST("max_snapshots set to 20", mgr4.max_snapshots == 20);
    TEST("auto_save_interval set to 10", mgr4.auto_save_interval == 10);

    /* --- Test 8: Edge cases --- */
    printf("\n[P98] Edge cases:\n");

    checkpoint_manager_t mgr5;
    checkpoint_init(&mgr5);

    /* Free without any checkpoints */
    checkpoint_free(&mgr5);
    TEST("free empty manager", 1);

    /* Restore from empty manager */
    agent_state_t *s3 = make_state();
    TEST("restore empty returns false",
         !checkpoint_restore(&mgr5, s3, "nonexistent"));
    free_state(s3);

    /* Autosave with empty state (no messages → returns false) */
    checkpoint_manager_t mgr6;
    checkpoint_init(&mgr6);
    agent_state_t *s4 = make_state(); /* message_count = 0 */
    mgr6.auto_save_interval = 1;
    mgr6.turn_counter = 0;
    TEST("autosave empty state returns false",
         !checkpoint_try_autosave(&mgr6, s4));
    /* turn_counter should still be 0 because check happens before increment */
    TEST("counter unchanged", mgr6.turn_counter == 0);
    TEST("no checkpoint saved", mgr6.count == 0);
    free_state(s4);

    /* --- Summary --- */
    free_state(state);
    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
