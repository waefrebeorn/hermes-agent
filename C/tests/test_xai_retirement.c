/* Test xAI model retirement detection (L04) */
#include "hermes_xai_retirement.h"
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

/* ================================================================
 *  Test normalize
 * ================================================================ */
static void test_normalize(void) {
    printf("\n=== Normalize ===\n");

    const char *r;

    r = hermes_xai_normalize("grok-4");
    TEST("bare grok-4", strcmp(r, "grok-4") == 0);

    r = hermes_xai_normalize("x-ai/grok-4");
    TEST("x-ai/ prefix stripped", strcmp(r, "grok-4") == 0);

    r = hermes_xai_normalize("xai/grok-4-fast");
    TEST("xai/ prefix stripped", strcmp(r, "grok-4-fast") == 0);

    r = hermes_xai_normalize("Grok-Code-Fast-1");
    TEST("lowercased", strcmp(r, "grok-code-fast-1") == 0);

    r = hermes_xai_normalize("  grok-4  ");
    TEST("whitespace trimmed", strcmp(r, "grok-4") == 0);

    r = hermes_xai_normalize(NULL);
    TEST("NULL returns empty", strcmp(r, "") == 0);

    r = hermes_xai_normalize("");
    TEST("empty returns empty", strcmp(r, "") == 0);
}

/* ================================================================
 *  Test looks_like
 * ================================================================ */
static void test_looks_like(void) {
    printf("\n=== Looks Like xAI ===\n");

    TEST("grok-4 prefix", hermes_xai_looks_like("grok-4"));
    TEST("x-ai/grok-4-1-fast", hermes_xai_looks_like("x-ai/grok-4-1-fast"));
    TEST("grok-code-fast-1", hermes_xai_looks_like("grok-code-fast-1"));

    TEST("gpt-4 not xAI", !hermes_xai_looks_like("gpt-4"));
    TEST("claude not xAI", !hermes_xai_looks_like("claude-sonnet-4-6"));
    TEST("openrouter not xAI", !hermes_xai_looks_like("openrouter/openai/gpt-4"));

    TEST("NULL not xAI", !hermes_xai_looks_like(NULL));
    TEST("empty not xAI", !hermes_xai_looks_like(""));
}

/* ================================================================
 *  Test empty config yields no issues
 * ================================================================ */
static void test_empty_config(void) {
    printf("\n=== Empty Config ===\n");

    hermes_config_t cfg = {0};
    xai_retirement_result_t r = hermes_xai_find_retired(&cfg);
    TEST("empty config, no issues", r.count == 0);

    r = hermes_xai_find_retired(NULL);
    TEST("NULL config, no issues", r.count == 0);
}

/* ================================================================
 *  Test non-xAI models are not flagged
 * ================================================================ */
static void test_non_xai_models(void) {
    printf("\n=== Non-xAI Models ===\n");

    hermes_config_t cfg = {0};
    snprintf(cfg.model, sizeof(cfg.model), "gpt-4o");
    snprintf(cfg.auxiliary.vision.model, sizeof(cfg.auxiliary.vision.model), "claude-sonnet-4-6");
    snprintf(cfg.delegation.child_model, sizeof(cfg.delegation.child_model), "openai/o3");

    xai_retirement_result_t r = hermes_xai_find_retired(&cfg);
    TEST("no xAI models, no issues", r.count == 0);
}

/* ================================================================
 *  Test valid xAI models are not flagged
 * ================================================================ */
static void test_valid_xai_models(void) {
    printf("\n=== Valid xAI Models (not retired) ===\n");

    hermes_config_t cfg = {0};
    snprintf(cfg.model, sizeof(cfg.model), "grok-4.3");
    snprintf(cfg.auxiliary.vision.model, sizeof(cfg.auxiliary.vision.model), "grok-4.20-0309-reasoning");
    snprintf(cfg.auxiliary.skills_hub.model, sizeof(cfg.auxiliary.skills_hub.model), "grok-4-fast");
    /* grok-4-fast is retired, should be caught. */
    /* Use a non-retired model for the rest */
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.model, sizeof(cfg.model), "grok-4.3");
    snprintf(cfg.auxiliary.compression.model, sizeof(cfg.auxiliary.compression.model), "grok-4.3");

    xai_retirement_result_t r = hermes_xai_find_retired(&cfg);
    TEST("valid models, no issues", r.count == 0);
}

/* ================================================================
 *  Test retired xAI models are detected
 * ================================================================ */
static void test_retired_detected(void) {
    printf("\n=== Retired Models Detected ===\n");

    hermes_config_t cfg = {0};
    snprintf(cfg.model, sizeof(cfg.model), "x-ai/grok-4");
    snprintf(cfg.auxiliary.vision.model, sizeof(cfg.auxiliary.vision.model), "grok-4-fast-non-reasoning");
    snprintf(cfg.delegation.child_model, sizeof(cfg.delegation.child_model), "grok-code-fast-1");
    snprintf(cfg.auxiliary.web_extract.model, sizeof(cfg.auxiliary.web_extract.model), "grok-imagine-image-pro");

    xai_retirement_result_t r = hermes_xai_find_retired(&cfg);
    TEST("4 retired models found", r.count == 4);

    /* Check first issue details */
    TEST("config path = principal.model", strcmp(r.issues[0].config_path, "principal.model") == 0);
    TEST("current model = x-ai/grok-4", strcmp(r.issues[0].current_model, "x-ai/grok-4") == 0);
    TEST("replacement = grok-4.3", strcmp(r.issues[0].replacement, "grok-4.3") == 0);

    /* Check non-reasoning variant has reasoning_effort=none */
    /* Issues: 0=principal, 1=delegation, 2=vision(non-reasoning), 3=web_extract */
    TEST("non-reasoning variant has reasoning_effort",
         strcmp(r.issues[2].reasoning_effort, "none") == 0);

    /* Check grok-imagine-image-pro replacement */
    TEST("image pro replacement",
         strcmp(r.issues[3].replacement, "grok-imagine-image-quality") == 0);
}

/* ================================================================
 *  Test format_issue
 * ================================================================ */
static void test_format_issue(void) {
    printf("\n=== Format Issue ===\n");

    char *msg = hermes_xai_format_issue(NULL);
    TEST("NULL issue returns NULL", msg == NULL);

    xai_retirement_issue_t issue;
    snprintf(issue.config_path, sizeof(issue.config_path), "principal.model");
    snprintf(issue.current_model, sizeof(issue.current_model), "grok-4");
    snprintf(issue.replacement, sizeof(issue.replacement), "grok-4.3");
    issue.reasoning_effort[0] = '\0';
    snprintf(issue.note, sizeof(issue.note), "ambiguous — defaulting to grok-4.3");

    msg = hermes_xai_format_issue(&issue);
    TEST("format returns message", msg != NULL);
    if (msg) {
        TEST("contains config path", strstr(msg, "principal.model") != NULL);
        TEST("contains retired model", strstr(msg, "grok-4") != NULL);
        TEST("contains replacement", strstr(msg, "grok-4.3") != NULL);
        TEST("contains note", strstr(msg, "ambiguous") != NULL);
        TEST("contains migration URL", strstr(msg, "docs.x.ai") != NULL);
        free(msg);
    }

    /* Format with reasoning_effort */
    snprintf(issue.reasoning_effort, sizeof(issue.reasoning_effort), "none");
    issue.note[0] = '\0';
    msg = hermes_xai_format_issue(&issue);
    TEST("format with reasoning_effort", msg != NULL);
    if (msg) {
        TEST("contains reasoning_effort", strstr(msg, "none") != NULL);
        free(msg);
    }
}

/* ================================================================
 *  Test all auxiliary slots
 * ================================================================ */
static void test_all_aux_slots(void) {
    printf("\n=== All Auxiliary Slots ===\n");

    hermes_config_t cfg = {0};
    snprintf(cfg.model, sizeof(cfg.model), "gpt-4o"); /* not xAI */
    snprintf(cfg.auxiliary.vision.model, sizeof(cfg.auxiliary.vision.model), "grok-4-fast");
    snprintf(cfg.auxiliary.web_extract.model, sizeof(cfg.auxiliary.web_extract.model), "grok-4-0709");
    snprintf(cfg.auxiliary.compression.model, sizeof(cfg.auxiliary.compression.model), "grok-4-fast-reasoning");
    snprintf(cfg.auxiliary.skills_hub.model, sizeof(cfg.auxiliary.skills_hub.model), "grok-4-1-fast");
    snprintf(cfg.auxiliary.approval.model, sizeof(cfg.auxiliary.approval.model), "grok-4-1-fast-non-reasoning");
    snprintf(cfg.auxiliary.mcp.model, sizeof(cfg.auxiliary.mcp.model), "grok-4-1-fast-reasoning");

    xai_retirement_result_t r = hermes_xai_find_retired(&cfg);
    TEST("6 auxiliary models retired", r.count == 6);

    /* Verify paths */
    TEST("vision path", strcmp(r.issues[0].config_path, "auxiliary.vision.model") == 0);
    TEST("web_extract path", strcmp(r.issues[1].config_path, "auxiliary.web_extract.model") == 0);
    TEST("compression path", strcmp(r.issues[2].config_path, "auxiliary.compression.model") == 0);
    TEST("skills_hub path", strcmp(r.issues[3].config_path, "auxiliary.skills_hub.model") == 0);
    TEST("approval path", strcmp(r.issues[4].config_path, "auxiliary.approval.model") == 0);
    TEST("mcp path", strcmp(r.issues[5].config_path, "auxiliary.mcp.model") == 0);
}

int main(void) {
    printf("=== xAI Retirement Detection Tests ===\n");

    test_normalize();
    test_looks_like();
    test_empty_config();
    test_non_xai_models();
    test_valid_xai_models();
    test_retired_detected();
    test_format_issue();
    test_all_aux_slots();

    printf("\n=== %d/%d passed ===\n", pass, pass + fail);
    return fail > 0 ? 1 : 0;
}
