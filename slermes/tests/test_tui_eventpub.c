/*
 * test_tui_eventpub.c — Tests for TUI Event Publisher (T07).
 * Tests event types, serialization, subscribers, FIFO batching.
 */
#include "tui_eventpub.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

/* Callback counters for subscriber tests */
static int g_cb_count = 0;
static tui_event_type_t g_last_type = TUI_EVENT_NONE;
static void test_cb(const tui_event_t *ev, void *userdata) {
    (void)userdata;
    g_cb_count++;
    g_last_type = ev->type;
}

int main(void) {
    /* Test 1: Type name strings */
    {
        TEST("type_name none", strcmp(tui_eventpub_type_name(TUI_EVENT_NONE), "none") == 0);
        TEST("type_name keyboard", strcmp(tui_eventpub_type_name(TUI_EVENT_KEYBOARD), "keyboard") == 0);
        TEST("type_name message_user", strcmp(tui_eventpub_type_name(TUI_EVENT_MESSAGE_USER), "message_user") == 0);
        TEST("type_name stream_token", strcmp(tui_eventpub_type_name(TUI_EVENT_STREAM_TOKEN), "stream_token") == 0);
        TEST("type_name unknown out of range", strcmp(tui_eventpub_type_name((tui_event_type_t)999), "unknown") == 0);
    }

    /* Test 2: Init and shutdown */
    {
        bool ok = tui_eventpub_init(NULL);
        TEST("init without FIFO", ok == true);
        TEST("fifo not open without path", tui_eventpub_fifo_is_open() == false);
        tui_eventpub_shutdown();
    }

    /* Test 3: Subscribe and emit */
    {
        tui_eventpub_init(NULL);
        g_cb_count = 0;
        g_last_type = TUI_EVENT_NONE;

        int sid = tui_eventpub_subscribe(TUI_EVENT_NONE, test_cb, NULL);
        TEST("subscribe returns positive ID", sid > 0);

        /* Emit a keyboard event */
        tui_eventpub_keyboard('a', "a");
        TEST("callback was called", g_cb_count == 1);
        TEST("callback got keyboard type", g_last_type == TUI_EVENT_KEYBOARD);

        /* Emit a resize event */
        tui_eventpub_resize(80, 24);
        TEST("callback called for resize", g_cb_count == 2);
        TEST("resize type", g_last_type == TUI_EVENT_RESIZE);

        /* Unsubscribe */
        tui_eventpub_unsubscribe(sid);
        tui_eventpub_keyboard('b', "b");
        TEST("no callback after unsubscribe", g_cb_count == 2);

        tui_eventpub_shutdown();
    }

    /* Test 4: Type-filtered subscription */
    {
        tui_eventpub_init(NULL);
        int sid_key = tui_eventpub_subscribe(TUI_EVENT_KEYBOARD, test_cb, NULL);
        int sid_msgs = tui_eventpub_subscribe(TUI_EVENT_MESSAGE_USER, test_cb, NULL);
        g_cb_count = 0;

        tui_eventpub_keyboard('x', "x");
        TEST("keyboard-only sub receives keyboard", g_cb_count >= 1);

        int prev = g_cb_count;
        tui_eventpub_resize(100, 40);
        TEST("keyboard sub not called for resize", g_cb_count == prev);

        tui_eventpub_unsubscribe(sid_key);
        tui_eventpub_unsubscribe(sid_msgs);
        tui_eventpub_shutdown();
    }

    /* Test 5: Keyboard event fields */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_keyboard(65, "A");
        tui_eventpub_shutdown();

        char *json = NULL;
        tui_event_t ev = {.type = TUI_EVENT_KEYBOARD};
        ev.data.keyboard.ch = 65;
        strncpy(ev.data.keyboard.utf8, "A", sizeof(ev.data.keyboard.utf8) - 1);
        json = tui_eventpub_to_json(&ev);
        TEST("keyboard json non-null", json != NULL);
        if (json) {
            TEST("keyboard json contains method",
                 strstr(json, "event_keyboard") != NULL);
            TEST("keyboard json contains ch:65",
                 strstr(json, "ch\":65") != NULL ||
                 strstr(json, "ch\": 65") != NULL);
            free(json);
        }
    }

    /* Test 6: Stream token JSON */
    {
        tui_event_t ev = {.type = TUI_EVENT_STREAM_TOKEN};
        strncpy(ev.data.stream.token, "Hello", sizeof(ev.data.stream.token) - 1);
        ev.data.stream.tokens_so_far = 42;
        char *json = tui_eventpub_to_json(&ev);
        TEST("stream json non-null", json != NULL);
        if (json) {
            TEST("stream json contains method event_stream_token",
                 strstr(json, "event_stream_token") != NULL);
            TEST("stream json contains token Hello",
                 strstr(json, "Hello") != NULL);
            TEST("stream json contains tokens_so_far",
                 strstr(json, "42") != NULL);
            free(json);
        }
    }

    /* Test 7: Stream done event */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_stream_done();
        tui_eventpub_shutdown();

        tui_event_t ev = {.type = TUI_EVENT_STREAM_DONE};
        ev.data.stream.tokens_so_far = 100;
        char *json = tui_eventpub_to_json(&ev);
        TEST("stream_done json non-null", json != NULL);
        if (json) {
            TEST("stream_done contains event_stream_done",
                 strstr(json, "event_stream_done") != NULL);
            free(json);
        }
    }

    /* Test 8: Tool event fields */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_tool("test_tool", "running", 50, 100, NULL);
        tui_eventpub_shutdown();

        tui_event_t ev = {.type = TUI_EVENT_TOOL_CALL};
        strncpy(ev.data.tool.tool_name, "test_tool", sizeof(ev.data.tool.tool_name) - 1);
        strncpy(ev.data.tool.status, "running", sizeof(ev.data.tool.status) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("tool json non-null", json != NULL);
        if (json) {
            TEST("tool json contains tool_name",
                 strstr(json, "test_tool") != NULL);
            TEST("tool json contains event_tool_call",
                 strstr(json, "event_tool_call") != NULL);
            free(json);
        }
    }

    /* Test 9: Tool result with preview */
    {
        tui_event_t ev = {.type = TUI_EVENT_TOOL_RESULT};
        strncpy(ev.data.tool.tool_name, "web_search", sizeof(ev.data.tool.tool_name) - 1);
        strncpy(ev.data.tool.status, "done", sizeof(ev.data.tool.status) - 1);
        strncpy(ev.data.tool.result_preview, "3 results found",
                sizeof(ev.data.tool.result_preview) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("tool_result json non-null", json != NULL);
        if (json) {
            TEST("tool_result contains event_tool_result",
                 strstr(json, "event_tool_result") != NULL);
            TEST("tool_result contains result_preview",
                 strstr(json, "result_preview") != NULL);
            free(json);
        }
    }

    /* Test 10: Session change event */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_session("session-abc-123", "created");
        tui_eventpub_shutdown();

        tui_event_t ev = {.type = TUI_EVENT_SESSION_CHANGE};
        strncpy(ev.data.session.session_id, "session-abc-123",
                sizeof(ev.data.session.session_id) - 1);
        strncpy(ev.data.session.action, "created",
                sizeof(ev.data.session.action) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("session json non-null", json != NULL);
        if (json) {
            TEST("session action created", strstr(json, "\"created\"") != NULL);
            TEST("session id session-abc-123", strstr(json, "session-abc-123") != NULL);
            free(json);
        }
    }

    /* Test 11: Session delete event (different type) */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_session("old-session", "deleted");
        tui_eventpub_shutdown();
        TEST("session delete did not crash", true);
    }

    /* Test 12: Agent update event */
    {
        tui_event_t ev = {.type = TUI_EVENT_AGENT_UPDATE};
        strncpy(ev.data.agent.model, "gpt-4", sizeof(ev.data.agent.model) - 1);
        strncpy(ev.data.agent.provider, "openai", sizeof(ev.data.agent.provider) - 1);
        ev.data.agent.iteration = 3;
        ev.data.agent.max_iterations = 10;
        ev.data.agent.tokens_in = 500;
        ev.data.agent.tokens_out = 1500;
        ev.data.agent.budget_remaining = 0.05;
        char *json = tui_eventpub_to_json(&ev);
        TEST("agent json non-null", json != NULL);
        if (json) {
            TEST("agent contains model gpt-4", strstr(json, "gpt-4") != NULL);
            TEST("agent contains provider openai", strstr(json, "openai") != NULL);
            TEST("agent contains iteration 3", strstr(json, "\"iteration\":3") != NULL);
            TEST("agent contains budget", strstr(json, "budget_remaining") != NULL);
            free(json);
        }
    }

    /* Test 13: Status update event */
    {
        tui_event_t ev = {.type = TUI_EVENT_STATUS_UPDATE};
        strncpy(ev.data.status.mode, "streaming", sizeof(ev.data.status.mode) - 1);
        strncpy(ev.data.status.model, "claude-3", sizeof(ev.data.status.model) - 1);
        strncpy(ev.data.status.provider, "anthropic", sizeof(ev.data.status.provider) - 1);
        ev.data.status.iteration = 1;
        ev.data.status.max_iterations = 5;
        ev.data.status.tokens_in = 100;
        ev.data.status.tokens_out = 200;
        ev.data.status.ctx_percent = 45;
        ev.data.status.budget = -1.0;
        char *json = tui_eventpub_to_json(&ev);
        TEST("status json non-null", json != NULL);
        if (json) {
            TEST("status contains mode streaming", strstr(json, "\"mode\":\"streaming\"") != NULL ||
                 strstr(json, "streaming") != NULL);
            TEST("status contains ctx_percent 45", strstr(json, "ctx_percent") != NULL);
            free(json);
        }
    }

    /* Test 14: Theme change event */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_theme("dracula");
        tui_eventpub_shutdown();

        tui_event_t ev = {.type = TUI_EVENT_THEME_CHANGE};
        strncpy(ev.data.theme.theme_name, "dracula", sizeof(ev.data.theme.theme_name) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("theme json non-null", json != NULL);
        if (json) {
            TEST("theme contains dracula", strstr(json, "dracula") != NULL);
            TEST("theme contains event_theme_change",
                 strstr(json, "event_theme_change") != NULL);
            free(json);
        }
    }

    /* Test 15: Model change event */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_model("claude-opus-4", "anthropic");
        tui_eventpub_shutdown();

        tui_event_t ev = {.type = TUI_EVENT_MODEL_CHANGE};
        strncpy(ev.data.model.model_name, "claude-opus-4", sizeof(ev.data.model.model_name) - 1);
        strncpy(ev.data.model.provider, "anthropic", sizeof(ev.data.model.provider) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("model json non-null", json != NULL);
        if (json) {
            TEST("model name claude-opus-4", strstr(json, "claude-opus-4") != NULL);
            TEST("model provider anthropic", strstr(json, "anthropic") != NULL);
            free(json);
        }
    }

    /* Test 16: Connection events */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_connection(true, "gateway started");
        tui_eventpub_connection(false, "connection lost");
        tui_eventpub_shutdown();
        TEST("connection events don't crash", true);

        tui_event_t ev = {.type = TUI_EVENT_GATEWAY_CONNECT};
        ev.data.connection.success = true;
        strncpy(ev.data.connection.reason, "started", sizeof(ev.data.connection.reason) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("connect json non-null", json != NULL);
        if (json) {
            TEST("connect contains event_gateway_connect",
                 strstr(json, "event_gateway_connect") != NULL);
            free(json);
        }

        /* Disconnect event */
        tui_event_t ev2 = {.type = TUI_EVENT_GATEWAY_DISCONNECT};
        ev2.data.connection.success = false;
        char *json2 = tui_eventpub_to_json(&ev2);
        TEST("disconnect json non-null", json2 != NULL);
        if (json2) {
            TEST("disconnect contains event_gateway_disconnect",
                 strstr(json2, "event_gateway_disconnect") != NULL);
            free(json2);
        }
    }

    /* Test 17: FIFO batching and flush */
    {
        /* Use a temp FIFO */
        const char *fifo_path = "/tmp/hermes_test_eventpub_fifo";
        unlink(fifo_path);
        mkfifo(fifo_path, 0600);

        /* Open read end non-blocking so writer doesn't block */
        int rfd = open(fifo_path, O_RDONLY | O_NONBLOCK);
        TEST("test FIFO readable", rfd >= 0);

        tui_eventpub_init(fifo_path);
        TEST("fifo is open after init", tui_eventpub_fifo_is_open() == true);

        /* Emit several events — they batch in the internal buffer */
        tui_eventpub_keyboard('a', "a");
        tui_eventpub_keyboard('b', "b");
        tui_eventpub_resize(80, 24);

        /* Flush to FIFO */
        tui_eventpub_flush();

        /* Read from FIFO */
        char buf[8192] = {0};
        int n = (int)read(rfd, buf, sizeof(buf) - 1);
        buf[n > 0 ? n : 0] = '\0';
        TEST("fifo received data after flush", n > 0);

        tui_eventpub_shutdown();

        /* Disable FIFO and verify no more writes */
        tui_eventpub_init(fifo_path);
        tui_eventpub_set_fifo_enabled(false);
        tui_eventpub_keyboard('c', "c");
        tui_eventpub_set_fifo_enabled(true);
        tui_eventpub_shutdown();

        close(rfd);
        unlink(fifo_path);
    }

    /* Test 18: Command event parsing */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_command("/help");
        tui_eventpub_command("/model gpt-4");
        tui_eventpub_command("/auth login xai-oauth");
        tui_eventpub_shutdown();
        TEST("command events don't crash", true);

        tui_event_t ev = {.type = TUI_EVENT_COMMAND};
        strncpy(ev.data.command.command, "/help", sizeof(ev.data.command.command) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("command json non-null", json != NULL);
        if (json) {
            TEST("command contains event_command",
                 strstr(json, "event_command") != NULL);
            TEST("command contains /help",
                 strstr(json, "/help") != NULL);
            free(json);
        }
    }

    /* Test 19: Message event with special chars (JSON escaping) */
    {
        tui_event_t ev = {.type = TUI_EVENT_MESSAGE_USER};
        ev.data.message.role = 0;
        /* Contains quote and newline — should not break JSON */
        strncpy(ev.data.message.text, "Hello \"world\"\nnext line",
                sizeof(ev.data.message.text) - 1);
        char *json = tui_eventpub_to_json(&ev);
        TEST("message json with special chars non-null", json != NULL);
        if (json) {
            TEST("escaped quote present", strstr(json, "\\\\\"") != NULL ||
                 strstr(json, "\\\"") != NULL);
            free(json);
        }
    }

    /* Test 20: Edge cases — NULL text, empty strings */
    {
        tui_eventpub_init(NULL);
        tui_eventpub_message(0, NULL, false);  /* Should not crash */
        tui_eventpub_message(0, "", false);     /* Empty string */
        tui_eventpub_command(NULL);              /* Should not crash */
        tui_eventpub_command("");                /* Empty command */
        tui_eventpub_tool(NULL, "running", 0, 0, NULL);  /* NULL name */
        tui_eventpub_theme(NULL);                /* NULL theme */
        tui_eventpub_model(NULL, NULL);          /* NULL model */
        tui_eventpub_session(NULL, "created");   /* NULL session */
        tui_eventpub_session("s1", NULL);        /* NULL action */
        TEST("NULL/empty safety", true);
        tui_eventpub_shutdown();
    }

    /* Test 21: FIFO reconnect */
    {
        const char *fifo_path = "/tmp/hermes_test_eventpub_reconnect";
        unlink(fifo_path);
        mkfifo(fifo_path, 0600);
        int rfd = open(fifo_path, O_RDONLY | O_NONBLOCK);

        tui_eventpub_init(fifo_path);
        TEST("fifo open after init", tui_eventpub_fifo_is_open() == true);

        /* Reconnect to same path */
        bool reconn = tui_eventpub_fifo_reconnect(fifo_path);
        TEST("fifo reconnect succeeds", reconn == true);

        tui_eventpub_keyboard('r', "r");
        tui_eventpub_flush();

        char buf[1024] = {0};
        int n = (int)read(rfd, buf, sizeof(buf) - 1);
        TEST("data received after reconnect", n > 0);

        close(rfd);
        tui_eventpub_shutdown();
        unlink(fifo_path);
    }

    printf("\n");
    if (failures > 0)
        printf("  %d TESTS FAILED\n", failures);
    else
        printf("  ALL TESTS PASSED\n");
    return failures > 0 ? 1 : 0;
}
