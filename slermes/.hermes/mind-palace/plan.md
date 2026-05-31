# Plan — Next Phase (v452)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/?/13.

**Latest:** Phase 396 — xAI Provider Test Expansion (S7 X03).
test_provider_xai.c: 69 new assertions (64→133 total). Coverage: URL edge
cases (double-slash fix, proxy), header edge cases (empty/NULL/long key),
response edge cases (empty/no choices, null content, length finish, no usage),
encrypted_content parsing, streaming edge depth (empty delta/chunk, length
finish reason, encrypted stream, whitespace), all 8 retired models verified
with correct replacements and reasoning_effort.
Bug fix: xai_parse_response — missing finish_reason extraction from choice level.

**Next gap target:**
- S7 Test coverage — next provider (Azure, Bedrock, OpenRouter, Custom)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+69 xAI provider tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
