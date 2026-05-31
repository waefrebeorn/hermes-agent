# Plan — Next Phase (v435)

S0: D09+D19+D20+D21 all PORTED. C11 PORTED. **T07+T06+T05+T01+T02+T08+T03 PORTED**.
56 gaps. Suite 334/0/13.

**Latest:** Phase 379 — TUI Render Engine (T03). Virtual screen with
dirty-rect tracking and double buffering. Text wrapping and automatic
scrolling. Color/attribute stacks for ncurses output. Full markdown
render pipeline supporting **bold**, `code`, # headers, indented code
blocks, role-based coloring. Flush to ncurses with incremental
dirty-region update. 16-test suite. S4 improved from 10→9 gaps.

**Next gap target:**
- S4 T04 TUI WebSocket support
- S7 Test coverage

**Structural gaps remaining by sector:**
- S4 TUI: 11 gaps (P1: T04; P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
