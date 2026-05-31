# Plan — Next Phase (v431)

S0: D09+D19+D20+D21 all PORTED. C11 PORTED. **T07+T06+T05 PORTED**.
60 gaps. Suite 330/0/13.

**Latest:** Phase 375 — TUI Entry/Startup Module (T05). Lifecycle
wrapper: pre-flight checks (TERM env, isatty, color support, minimum
size), SIGTERM handler with atomic stop flag, startup result codes
with human-readable messages, exit reason tracking. Graceful
degradation for terminal too small / no color / no TERM. 
10-test suite (test_tui_entry.c). S4 depth improved from 14→13 gaps.

**Next gap target:**
- S4 TUI backend (T01-T04, T08)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 15 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
