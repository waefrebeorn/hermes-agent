# Plan — Next Phase (v429)

S0: D09+D19+D20+D21 all PORTED. **C11 PORTED** (xAI OAuth callback login).
**T07 PORTED** (TUI Event Publisher). 62 gaps. Suite 328/0/13.

**Latest:** Phase 373 — TUI Event Publisher (T07). Typed event system
with 22 event types, JSON-RPC 2.0 serialization, FIFO output batcher,
subscriber dispatch with type filters. Wired into tui_fullscreen.c:
history messages, streaming tokens, tool status, status updates,
resize, command input. 21-test suite (test_tui_eventpub.c).
S4 depth improved from 16→15 gaps.

**Next gap target:**
- S4 TUI backend (T01-T06, T08: JSON-RPC, transport, render, WebSocket, entry, slash worker, app layout)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 15 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
