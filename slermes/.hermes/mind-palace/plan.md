# Plan — Next Phase (v426)

S0: D09 PORTED, D21 PORTED (live timestamp). 66 gaps. Suite 328/0/12.

**Latest:** Phase 370 — Live HH:MM timestamp on CLI + TUI status bars (replaced raw session_id display). Two new display gaps discovered: D19 (context usage % in status bar), D20 (budget/cost in CLI status bar).

**Next gap target:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P2 | S0 D19 | Context % in status bar | Add context_pct using config max_context or provider metadata |
| P2 | S0 D20 | Budget/cost in CLI status bar | Add budget/cost display to display_core.c status bar |

**Structural gaps remaining by sector:**
- S0 Display: 2 gaps (D19 context%, D20 budget/cost)
- S4 TUI: 16 gaps (P1: T01-T08; P2-P3: T19-T28)
- S5 CLI: 11 gaps (C11 auth PARTIAL, C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
