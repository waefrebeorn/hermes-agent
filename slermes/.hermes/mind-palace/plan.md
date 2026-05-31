# Plan — Next Phase (v443)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/0/13.

**Latest:** Phase 387 — Conversation Loop Edge Case Expansion (Round 2, S7 X08).
test_conversation_edge.c: 28 new assertions (48→76 total). Coverage:
hermes_message_sanitize (NULL/empty/think blocks/redact/surrogates/tool args),
message_free (NULL/empty/content), additional repair (3 consecutive users,
cross-assistant tool IDs, empty tool ID, user-after-tool, mixed roles, all
tool IDs mismatched).

**Next gap target:**
- S7 Test coverage — next cluster (X03 provider tests or X10 fuzz)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+28 conversation edge tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
