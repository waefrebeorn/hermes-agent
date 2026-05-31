# Plan — Next Phase (v440)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/0/13.

**Latest:** Phase 384 — Agent Loop Core Function Tests (S7 X06).
test_agent_loop.c: 90 new assertions covering session_id format,
agent_free, agent_configure_from_config, agent_inject_history,
agent_snapshot lifecycle. 6 test groups, 15 test functions.
S7 X06 agent loop core test parity improved.

**Next gap target:**
- S7 Test coverage — next cluster (X07 gateway platform tests)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+90 agent loop tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
