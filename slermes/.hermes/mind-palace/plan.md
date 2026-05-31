# Plan — Next Phase (v444)

S0-S3+S6+S8 all PORTED. S4: all P1 gaps resolved.
**T09-T18+T07+T06+T05+T01+T02+T08+T03+T04 PORTED.**
53 gaps. Suite 338/0/13.

**Latest:** Phase 388 — Fuzz Test Expansion (Round 2, S7 X10).
test_fuzz.c: 5 new assertions (15→20 total). Coverage: HTML event handlers,
nested scripts, style/conditional comments, 100-level deep nesting. Path
home dir/over-traversal/complex mixed/Windows UNC/4096-char. Regex
escaped bracket/alternation/long chain/email. Template lone endif/
unclosed if/var/unknown tag. JSON numeric edge/50-level recursive array.

**Next gap target:**
- S7 Test coverage — next cluster (X03 provider depth or X11 performance)
- S4 T19-T28 React tsx parity (P2-P3)
- S5 CLI ecosystem (P2)

**Structural gaps remaining by sector:**
- S4 TUI: 8 gaps (P2-P3: T19-T28)
- S5 CLI: 10 gaps (C19-C30 REAL GAP)
- S7 Tests: 18 clusters (+5 fuzz tests)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all architectural)
