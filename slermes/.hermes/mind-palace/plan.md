# Plan — Next Phase (v454)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/?/13.

**Latest:** Phase 398 — Bedrock Provider Test Expansion (S7 X03).
test_provider_bedrock.c: 61 new assertions (41→102 total). Coverage: stop
reason mapping (end_turn→stop, stop_sequence→stop, tool_use→tool_calls,
max_tokens→length, content_filtered→content_filter, guardrail_intervened→
content_filter, unknown→stop, missing→empty); error classification
(bedrock_is_context_overflow: 3 patterns with 15 cases;
bedrock_classify_error: context_overflow/rate_limit/overloaded/unknown);
response edge cases (no output, no message, empty content, tool-only content);
URL edge cases (NULL base, empty model); streaming (NULL, empty).

**Next gap target:**
- S7 Test coverage — next provider (OpenRouter, Custom)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+61 Bedrock provider tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
