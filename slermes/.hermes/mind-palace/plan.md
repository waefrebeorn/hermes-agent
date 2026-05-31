# Plan — Next Phase (v420)

S0+S1+S3+S6+R02+R04+R10 PORTED. F10 PORTED. 67 gaps. Suite 328/0/12.
Upstream version dynamically extracted from Python at build time.

**Latest:** Phase 363 — TUI improvements: right-aligned thinking indicator, live token counter during streaming, Ctrl+C abort (SIGINT handler), non-blocking input during entire streaming period.

**Next gap target:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P1 | S4 TUI | T11 thinking indicator | Animated spinner + live counter + abort done (Phase 363). Remaining: agent overlay (T14), todo panel (T15), rich animated states. |
| P1 | S5 C11 | Auth CLI | Remaining: xAI callback flow. |

**Structural gaps remaining by sector:**
- S4 TUI: 20 gaps (P1: T01-T08, T11, T14, T18; P2-P3: rest) — T09+T10+T12+T13+T16+T17 PORTED
- S5 CLI: 11 gaps (C11 auth PARTIAL, C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1 — halted)
- S9 Plugin: 19 gaps (P01 PARTIAL)
- S10 Arch: 7 gaps (all 🏛️)
