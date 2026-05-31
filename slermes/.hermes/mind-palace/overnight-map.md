# Overnight Map (v453)

## Phase 397 — Azure Provider Test Expansion
**S7 X03 EXPANDED** — 40 new assertions in test_provider_azure.c (54→94 total)
**Coverage:** URL edge cases (proxy URL preserved with /deployments/, custom
model name in URL, empty base→default azure.com); header edge cases (empty key
omits api-key, NULL key omits, long key included); response edge cases (empty
choices array, no choices key, null content→empty string, length finish reason
with finish_reason extracted, no usage metadata→zero tokens); streaming edge
depth (empty delta, empty chunk, length finish reason in stream, whitespace
delta preserved).
**Bug fix:** azure_parse_response — missing finish_reason extraction from choice
level (same pattern as xAI Phase 396). The streaming path already had it.
**Suite:** 338/?/13 — Stable
**Gaps:** 53 (depth improved, no count reduction)

## Files Modified
- tests/test_provider_azure.c — +40 assertions, 4 new test functions
- src/agent/provider_azure.c — bug fix (finish_reason in parse_response)
- test_runner.sh — label 54→94
- .hermes/mind-palace/ — all walkways bumped v453

## Phases This Session
391: Delegate edge cases (S7 X04)
392: OpenAI provider tests (S7 X03) — 2 bug fixes
393: Anthropic provider tests (S7 X03)
394: DeepSeek provider tests (S7 X03)
395: Google provider tests (S7 X03)
396: xAI provider tests (S7 X03) — 1 bug fix
397: Azure provider tests (S7 X03) — 1 bug fix
