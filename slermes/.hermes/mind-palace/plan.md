# Plan — Next Phase (v417)

S0+S1+S3+S6+R02+R04+R10 PORTED. F10 PORTED. 67 gaps. Suite 328/0/12.
Upstream drift: ZERO (all differences are slermes/ additions, not upstream changes).
C-based cron job replaces Python LLM-agent cron — zero token cost.

**Next gap target:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P1 | S5 C11 | Auth/OAuth CLI | Add interactive `/auth login <provider>` with browser-open callback flow, reusing libmcp_oauth infrastructure. Currently `/auth status|providers|login|tokens|refresh` done. Device code flow for Nous (Phase 359). Token refresh + listing (Phase 360). Remaining: xAI callback flow, credential validation. |

**Structural gaps remaining by sector:**
- S4 TUI: 24 gaps (P1: T01-T11, T14, T18; P2-P3: rest)
- S5 CLI: 11 gaps (1 P1: C11 auth PARTIAL; 10 P2-P3 — C19-C30 REAL GAP)
- S7 Tests: 19 clusters (P1 — halted by user direction)
- S9 Plugin: 20 gaps (1 P1: P01 plugin lifecycle; rest P2-P3)
- S10 Arch: 7 gaps (all 🏛️ architectural)
