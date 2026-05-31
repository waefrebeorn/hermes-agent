# Overnight Map (v451)

## Phase 395 — Google Provider Test Expansion
**S7 X03 EXPANDED** — 87 new assertions in test_provider_google.c (65→152 total)
**Coverage:** Finish reason mapping (9 Google reasons: STOP→stop, MAX_TOKENS→length,
SAFETY→content_filter, RECITATION→content_filter, BLOCKLIST→content_filter,
PROHIBITED_CONTENT→content_filter, SPAM→content_filter, IMAGE_SAFETY→content_filter,
OTHER→stop); content blocked message when no text + safety finish; is_native_base_url
(standard, /openai compat, custom, NULL, empty); coerce_content_to_text (NULL, string,
empty, array of strings, text objects, empty array); URL edge cases (stream endpoint,
proxy, empty model); header edge cases (empty/NULL/long key); streaming finish reason
depth (SAFETY→content_filter, MAX_TOKENS→length, SPAM→content_filter); empty candidates
(empty array, usage only, no content).
**Suite:** 338/?/13 — Stable
**Gaps:** 53 (depth improved, no count reduction)

## Files Modified
- tests/test_provider_google.c — +87 assertions, 8 new test functions
- test_runner.sh — label 64→152
- .hermes/mind-palace/ — all walkways bumped v451

## Phases This Session
391: Delegate edge cases (S7 X04)
392: OpenAI provider tests (S7 X03)
393: Anthropic provider tests (S7 X03)
394: DeepSeek provider tests (S7 X03)
395: Google provider tests (S7 X03)
