# Plan — Next Phase (v442)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/0/13.

**Latest:** Phase 386 — Conversation Loop Edge Case Expansion (S7 X08).
test_conversation_edge.c: 19 new assertions (29→48 total). Coverage:
message sequence edge cases (consecutive assistants, tool-only, system-only,
assistant-before-system, duplicate tool IDs, null names, long IDs),
NULL safety, negative/zero count. S7 X08 parity improved.

**Next gap target:**
- S7 Test coverage — next cluster (X03 provider tests or X10 fuzz)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+19 conversation edge tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
