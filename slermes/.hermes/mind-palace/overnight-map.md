# Overnight Map (v461)

## Phase 405 — Discord Tool Edge Case Expansion
**S7 X04 EXPANDED** — 6 new test functions in test_discord.c (13→19 tests)
New coverage: empty action (''→Missing action), fetch_messages missing channel,
search_members missing guild_id/query, pin/unpin missing channel/message,
create_thread missing name, role mgmt missing user/role_id
no usage), streaming reasoning (reasoning_content in delta), streaming edge
depth (empty delta/chunk, length finish, whitespace).
**Bug fix:** openrouter_parse_response — finish_reason extraction from choice
level (same pattern as xAI/Azure phases).
**Suite:** 338/?/13 — Stable
**Gaps:** 53 (depth improved)

## Files Modified
- tests/test_provider_openrouter.c — +43 assertions, 5 new test functions
- src/agent/provider_openrouter.c — bug fix (+3 lines)
- test_runner.sh — label 50→94

## Phases This Session
391: Delegate (S7 X04)
392: OpenAI (S7 X03) — 2 bug fixes
393: Anthropic (S7 X03)
394: DeepSeek (S7 X03)
395: Google (S7 X03)
396: xAI (S7 X03) — 1 bug fix
397: Azure (S7 X03) — 1 bug fix
398: Bedrock (S7 X03)
399: OpenRouter (S7 X03) — 1 bug fix
