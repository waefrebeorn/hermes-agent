# Overnight Map (v454)

## Phase 398 — Bedrock Provider Test Expansion
**S7 X03 EXPANDED** — 61 new assertions in test_provider_bedrock.c (41→102 total)
**Coverage:** Stop reason mapping (end_turn→stop, stop_sequence→stop,
tool_use→tool_calls, max_tokens→length, content_filtered→content_filter,
guardrail_intervened→content_filter, unknown_reason→stop, missing→"");
error classification: bedrock_is_context_overflow (15 cases across 3
patterns: validation+too_long, validation+exceeds+max_tokens,
stream_error+too_long + non-matches), bedrock_classify_error (10 cases:
context_overflow, rate_limit 3 patterns, overloaded 4 patterns, unknown, NULL);
response edge cases (no output, no message, empty content→NULL,
tool-only content→empty text+tool count); URL edge cases (NULL base→default
region, empty model→default model); streaming (NULL→"", empty→"").
**Suite:** 338/?/13 — Stable
**Gaps:** 53 (depth improved)

## Files Modified
- tests/test_provider_bedrock.c — +61 assertions, 6 new test functions
- test_runner.sh — label 40→102
- .hermes/mind-palace/ — all walkways bumped v454

## Phases This Session
391: Delegate (S7 X04)
392: OpenAI (S7 X03) — 2 bug fixes
393: Anthropic (S7 X03)
394: DeepSeek (S7 X03)
395: Google (S7 X03)
396: xAI (S7 X03) — 1 bug fix
397: Azure (S7 X03) — 1 bug fix
398: Bedrock (S7 X03)
