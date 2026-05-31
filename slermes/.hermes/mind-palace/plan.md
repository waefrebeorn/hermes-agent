# Plan — Next Phase (v427)

S0: D09 PORTED, D21 PORTED, **D19 PORTED, D20 PORTED** (context% + budget/cost in CLI status bar). 64 gaps. Suite 327/0/13.

**Latest:** Phase 371 — Context usage %, budget (N/M iterations), and estimated cost ($X.XX) now shown in CLI status bar via `hermes_token_context_size()` and agent state fields. S0 display sector now 0 gaps.

**Next gap target:**
- S4 TUI backend (T01-T08: JSON-RPC, transport, render, WebSocket, entry, slash worker, event publisher, app layout)
- S5 CLI ecosystem (C11 auth PARTIAL — callback server remaining)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 16 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 11 gaps (C11 auth PARTIAL, C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
