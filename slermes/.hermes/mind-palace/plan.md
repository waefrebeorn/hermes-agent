# Plan — Next Phase (v430)

S0: D09+D19+D20+D21 all PORTED. C11 PORTED. **T07+T06 PORTED**.
61 gaps. Suite 329/0/13.

**Latest:** Phase 374 — TUI Slash Command Worker (T06). Dispatch-table
architecture with 30 registered commands across 6 categories (TUI,
Agent, Session, Modal, Meta, Skills). Argument parsing with quote
support, programmatic register/unregister, categorized help generation.
Replaced tui_process_input's 200-line if/else chain with single
tui_slash_dispatch() call. 20-test suite (test_tui_slash_worker.c).
S4 depth improved from 15→14 gaps.

**Next gap target:**
- S4 TUI backend (T01-T05, T08)
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 15 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
