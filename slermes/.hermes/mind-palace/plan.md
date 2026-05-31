# Plan — Next Phase (v433)

S0: D09+D19+D20+D21 all PORTED. C11 PORTED. **T07+T06+T05+T01+T02 PORTED**.
58 gaps. Suite 332/0/13.

**Latest:** Phase 377 — TUI Transport Layer (T02). FIFO-based transport
abstraction: connection state machine (IDLE/CONNECTING/CONNECTED/
DISCONNECTED/ERROR), configurable reconnection (retry count + delay),
state change callbacks, message framing (newline-delimited), poll-based
I/O, send/recv/sendf/send_rpc API, shutdown with FIFO cleanup.
13-test suite. S4 depth improved from 12→11 gaps.

**Next gap target:**
- S4 TUI backend (T03, T04, T08)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 13 gaps (P1: T03, T04, T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
