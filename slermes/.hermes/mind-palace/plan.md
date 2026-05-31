# Plan — Next Phase (v422)

S0+S1+S3+S6+R02+R04+R10 PORTED. F10 PORTED. 66 gaps. Suite 328/0/12.
Upstream version dynamically extracted from Python at build time.

**Latest:** Phase 366 — TUI type-ahead buffer (T18): keystrokes now captured during streaming and replayed into input buffer after stream ends. Previously just beeped and discarded.

**Next gap target:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P2 | S4 TUI | T14 | Agents overlay — show/switch between agents |
| P1 | S5 C11 | Auth CLI | Remaining: xAI callback flow. |

**Structural gaps remaining by sector:**
- S4 TUI: 18 gaps (P1: T01-T08, T11, T14; P2-P3: rest) — T09+T10+T12+T13+T15+T16+T17+T18 PORTED
- S5 CLI: 11 gaps (C11 auth PARTIAL, C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1 — halted)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all 🏛️)
