# Plan — Next Phase (v424)

S0+S1+S3+S6+R02+R04+R10 PORTED. F10 PORTED. 64 gaps. Suite 328/0/12.
Upstream version dynamically extracted from Python at build time.

**Latest:** Phase 368 — TUI agent info overlay (T14): /agent opens modal with model, provider, session, tokens, budget, JSON mode. Read-only info display.

**Next gap target:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P1 | S4 TUI | T01 | TUI JSON-RPC gateway server — wire FIFO transport to agent |
| P1 | S5 C11 | Auth CLI | Remaining: xAI callback flow. |

**Structural gaps remaining by sector:**
- S4 TUI: 16 gaps (P1: T01-T08; P2-P3: T19-T28) — T09+T10+T11+T12+T13+T14+T15+T16+T17+T18 PORTED
- S5 CLI: 11 gaps (C11 auth PARTIAL, C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1 — halted)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all 🏛️)
