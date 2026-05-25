/*
 * test_tool_backend.c — Tests for lib/libtoolbackend/tool_backend.c
 */

#include "tool_backend.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { printf("  %s ... ", name); } while(0)
#define PASS() do { printf("PASS\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); g_fail++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ─── Test: managed_nous_tools_enabled ─────────────────── */

static void test_nous_enabled_false_by_default(void)
{
    TEST("nous disabled by default");
    unsetenv("NOUS_MANAGED_TOOLS");
    ASSERT(!managed_nous_tools_enabled(), "should be false without env var");
    PASS();
}

static void test_nous_enabled_true_when_set(void)
{
    TEST("nous enabled when NOUS_MANAGED_TOOLS=true");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    ASSERT(managed_nous_tools_enabled(), "should be true");
    PASS();
}

static void test_nous_enabled_false_when_false(void)
{
    TEST("nous disabled when NOUS_MANAGED_TOOLS=false");
    setenv("NOUS_MANAGED_TOOLS", "false", 1);
    ASSERT(!managed_nous_tools_enabled(), "should be false");
    PASS();
}

/* ─── Test: normalize_browser_cloud_provider ────────────── */

static void test_browser_provider_default(void)
{
    TEST("browser provider default is local");
    const char *r = normalize_browser_cloud_provider(NULL);
    ASSERT(strcmp(r, "local") == 0, "default should be 'local'");
    PASS();
}

static void test_browser_provider_local(void)
{
    TEST("browser provider accepts 'local'");
    const char *r = normalize_browser_cloud_provider("local");
    ASSERT(strcmp(r, "local") == 0, "should return 'local'");
    PASS();
}

static void test_browser_provider_unknown(void)
{
    TEST("browser provider falls back to local for unknown");
    const char *r = normalize_browser_cloud_provider("aws");
    ASSERT(strcmp(r, "local") == 0, "unknown provider should fall back to local");
    PASS();
}

/* ─── Test: coerce_modal_mode ──────────────────────────── */

static void test_modal_mode_default(void)
{
    TEST("modal mode default is auto");
    const char *r = coerce_modal_mode(NULL);
    ASSERT(strcmp(r, "auto") == 0, "default should be 'auto'");
    PASS();
}

static void test_modal_mode_direct(void)
{
    TEST("modal mode accepts 'direct'");
    const char *r = coerce_modal_mode("direct");
    ASSERT(strcmp(r, "direct") == 0, "should return 'direct'");
    PASS();
}

static void test_modal_mode_unknown(void)
{
    TEST("modal mode falls back to auto for unknown");
    const char *r = coerce_modal_mode("unknown");
    ASSERT(strcmp(r, "auto") == 0, "unknown mode should fall back to auto");
    PASS();
}

/* ─── Test: has_direct_modal_credentials ───────────────── */

static void test_modal_direct_no_creds(void)
{
    TEST("modal direct creds false without env vars");
    unsetenv("MODAL_TOKEN_ID");
    unsetenv("MODAL_TOKEN_SECRET");
    ASSERT(!has_direct_modal_credentials(), "should be false");
    PASS();
}

static void test_modal_direct_with_env_vars(void)
{
    TEST("modal direct creds true with env vars");
    setenv("MODAL_TOKEN_ID", "test-id", 1);
    setenv("MODAL_TOKEN_SECRET", "test-secret", 1);
    ASSERT(has_direct_modal_credentials(), "should be true");
    PASS();
}

/* ─── Test: resolve_modal_backend_state ────────────────── */

static void test_modal_resolve_auto_managed(void)
{
    TEST("modal resolve auto prefers managed");
    setenv("NOUS_MANAGED_TOOLS", "true", 1);
    modal_backend_state_t s = resolve_modal_backend_state("auto", false, true);
    ASSERT(s.selected_backend != NULL, "selected_backend should not be NULL");
    ASSERT(strcmp(s.selected_backend, "managed") == 0, "auto should select managed");
    ASSERT(!s.managed_mode_blocked, "managed should not be blocked");
    PASS();
}

static void test_modal_resolve_auto_direct(void)
{
    TEST("modal resolve auto falls back to direct");
    unsetenv("NOUS_MANAGED_TOOLS");
    modal_backend_state_t s = resolve_modal_backend_state("auto", true, false);
    ASSERT(s.selected_backend != NULL, "selected_backend should not be NULL");
    ASSERT(strcmp(s.selected_backend, "direct") == 0, "auto should fall back to direct");
    PASS();
}

static void test_modal_resolve_direct_blocked(void)
{
    TEST("modal resolve direct blocked without creds");
    modal_backend_state_t s = resolve_modal_backend_state("direct", false, false);
    ASSERT(s.selected_backend == NULL, "selected_backend should be NULL");
    PASS();
}

static void test_modal_resolve_managed_blocked(void)
{
    TEST("modal resolve managed blocked without nous");
    unsetenv("NOUS_MANAGED_TOOLS");
    modal_backend_state_t s = resolve_modal_backend_state("managed", false, true);
    ASSERT(s.selected_backend == NULL, "selected_backend should be NULL");
    ASSERT(s.managed_mode_blocked, "managed_mode_blocked should be true");
    PASS();
}

/* ─── Test: resolve_openai_audio_api_key ───────────────── */

static void test_audio_key_fallback(void)
{
    TEST("audio key prefers voice tools key");
    setenv("VOICE_TOOLS_OPENAI_KEY", "voice-key-123", 1);
    unsetenv("OPENAI_API_KEY");
    tool_backend_reset();
    const char *k = resolve_openai_audio_api_key();
    ASSERT(strcmp(k, "voice-key-123") == 0, "should return voice key");
    PASS();
}

static void test_audio_key_openai_fallback(void)
{
    TEST("audio key falls back to OPENAI_API_KEY");
    unsetenv("VOICE_TOOLS_OPENAI_KEY");
    setenv("OPENAI_API_KEY", "openai-key-456", 1);
    tool_backend_reset();
    const char *k = resolve_openai_audio_api_key();
    ASSERT(strcmp(k, "openai-key-456") == 0, "should return openai key");
    PASS();
}

/* ─── Test: fal_key_configured ──────────────────────────── */

static void test_fal_key_default(void)
{
    TEST("fal key false without env");
    unsetenv("SLERMES_FAL_KEY");
    unsetenv("FAL_KEY");
    ASSERT(!fal_key_configured(), "should be false");
    PASS();
}

static void test_fal_key_with_slermes(void)
{
    TEST("fal key true with SLERMES_FAL_KEY");
    setenv("SLERMES_FAL_KEY", "fal-key-789", 1);
    ASSERT(fal_key_configured(), "should be true");
    PASS();
}

/* ─── Main ──────────────────────────────────────────────── */

int main(void)
{
    printf("=== tool_backend tests ===\n\n");

    /* Nous managed tools */
    test_nous_enabled_false_by_default();
    test_nous_enabled_true_when_set();
    test_nous_enabled_false_when_false();

    /* Browser provider */
    test_browser_provider_default();
    test_browser_provider_local();
    test_browser_provider_unknown();

    /* Modal mode */
    test_modal_mode_default();
    test_modal_mode_direct();
    test_modal_mode_unknown();

    /* Modal credentials */
    test_modal_direct_no_creds();
    test_modal_direct_with_env_vars();

    /* Modal backend resolution */
    test_modal_resolve_auto_managed();
    test_modal_resolve_auto_direct();
    test_modal_resolve_direct_blocked();
    test_modal_resolve_managed_blocked();

    /* Audio key */
    test_audio_key_fallback();
    test_audio_key_openai_fallback();

    /* FAL key */
    test_fal_key_default();
    test_fal_key_with_slermes();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
