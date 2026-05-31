# Plan — Next Phase (v451)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/?/13.

**Latest:** Phase 395 — Google Provider Test Expansion (S7 X03).
test_provider_google.c: 87 new assertions (65→152 total). Coverage: finish
reason mapping (9 Google reasons), content blocked message (SAFETY/BLOCKLIST/
PROHIBITED_CONTENT), is_native_base_url (7 cases), coerce_content_to_text
(NULL, string, empty, array, text objects, empty array), URL edge cases
(stream endpoint, proxy, empty model), header edge cases (empty/NULL/long key),
streaming finish reason depth (SAFETY/MAX_TOKENS/SPAM), empty candidates
(empty array, usage only, no content).

**Next gap target:**
- S7 Test coverage — next provider (xAI, Azure, Bedrock, OpenRouter, Custom)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+87 Google provider tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
