# Overnight Map (v450)

## Phase 394 — DeepSeek Provider Test Expansion
**S7 X03 EXPANDED** — 35 new assertions in test_provider_deepseek.c (71→106 total)
**Coverage:** URL edge cases (trailing slash, proxy, empty base), header edge cases
(empty key, negative cache TTL), tool call parsing, reasoning+tool call combined,
streaming edge cases (length/content_filter finish, role delta, content+reasoning,
whitespace, empty), FIM response edge cases (empty text, no choices, invalid JSON),
FIM body edge cases (empty prompt/suffix, long prompt, zero max_tokens),
response edge cases (empty/no/null choices).
**Suite:** 338/?/13 — Awaiting full run
**Gaps:** 53 (depth improved, no count reduction)

## Files Modified
- tests/test_provider_deepseek.c — +35 assertions, 8 new test functions
- test_runner.sh — label 60→106
- .hermes/mind-palace/ — all walkways bumped v450

## Phases This Session
391: Delegate edge cases (S7 X04)
392: OpenAI provider tests (S7 X03)
393: Anthropic provider tests (S7 X03)
394: DeepSeek provider tests (S7 X03)
