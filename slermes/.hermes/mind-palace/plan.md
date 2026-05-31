# Plan — Next Phase (v448)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/?/13.

**Latest:** Phase 392 — Provider Test Expansion (S7 X03).
test_provider_openai.c: 74 new assertions (37→111 total). Coverage: multi-tool
calls, empty/null content, streaming edge cases, build_request_body (basic + tools),
URL edge cases, finish_reason parsing.
**Fixed:** finish_reason extraction in parse_response (was missing), reasoning_content
streaming in parse_stream_chunk (was missing).

**Next gap target:**
- S7 Test coverage — next cluster (X05 integration or X11 performance)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+74 OpenAI provider tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
