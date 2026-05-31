# Plan — Next Phase (v449)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/?/13.

**Latest:** Phase 393 — Anthropic Provider Test Expansion (S7 X03).
test_provider_anthropic.c: 70 new assertions (28→98 total). Coverage: response
parsing (text, tool_use, multi-tool, empty, null, error, malformed), streaming
(text_delta, block events, message_delta, ping, [DONE]), URL edge cases,
headers edge cases (NULL key, OAuth token, cached).
build_request_body token: deferred (provider registration pipeline issue).

**Next gap target:**
- S7 Test coverage — next provider (DeepSeek, Google, xAI, Azure, Bedrock)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+70 Anthropic provider tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
