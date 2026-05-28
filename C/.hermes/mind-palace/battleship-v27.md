# Battle Map v27 — Real Parity Assessment + Upstream Drift

**v116 | 276 C commits ahead, 7583 upstream commits behind (fork: 2517917de)**
**Honest assessment: 33 gaps across 5 sectors.**

## S0: Form-vs-Function Parity (P0)

| # | ID | Issue | C State | Python State | Priority |
|---|----|-------|---------|-------------|----------|
| 1 | F01 | Single-turn-only agent | Works for one exchange, retry broken in TUI | Full multi-turn + retry | P0 |
| 2 | F02 | Setup not 1:1 | Partial env var setup, missing init flow parity | Full interactive setup wizard | P0 |
| 3 | F03 | Terminal resize race | SIGWINCH handler missing or unreliable | Clean resize handling | P0 |
| 4 | F04 | C can't hook Python | Standalone C, cannot import Python modules | Python is source of truth | P0 |
| 5 | F05 | Test cheating | 239 C test files vs ~17k Python tests | Full behavioral test suite | P0 |

## S1: Pipeline & Integration (P1)

| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | P01 | Retry broken in TUI | Agent fails on retry in terminal UI context | P1 |
| 2 | P02 | Plumbing edge cases | Integration between components has bugs | P1 |
| 3 | P03 | Linkage/dependency integrity | Dependency wiring may cut corners vs Python | P1 |
| 4 | P04 | TUI display bugs | Display/input bugs in terminal UI | P1 |
| 5 | P05 | General usage bugs | Behavioral bugs in normal operation | P1 |

## S2: Cross-Comparison (P1)

| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | A01 | Full AST tool comparison | Every Python tool vs C equivalent | P1 |
| 2 | A02 | Test suite recreation | 17k Python tests → C equivalents | P1 |
| 3 | A03 | Behavioral parity | Same input → same output for all tools | P1 |
| 4 | A04 | JSON schema parity | Tool schemas must match Python exactly | P1 |

## S3: Product Features (P2)

| # | ID | Feature | Details | Priority |
|---|----|---------|---------|----------|
| 1 | Q01 | Multi-turn conversation | Full back-and-forth with memory | P2 |
| 2 | Q02 | Session persistence | Full SQLite session save/restore | P2 |
| 3 | Q03 | Plugin system | Dynamic .so loading matching Python plugin model | P2 |
| 4 | Q04 | Skin engine parity | All Python skins available | P2 |
| 5 | Q05 | Gateway platform parity | All 19+ platforms fully tested | P2 |
| 6 | Q06 | Provider mode parity | All providers: stream, thinking, caching, oauth | P2 |

## S4: Upstream Drift (P1) — NEW

7583 upstream commits since fork point 2517917de. Categorized by impact area:

| # | ID | Topic | Upstream Changes | C Impact | Priority |
|---|-----|-------|-----------------|----------|----------|
| 1 | U01 | Provider/API changes | XAI retry 429 handling, OAuth fixes, auth fallback, entitlement refresh, xAI proxy rate-limit handling, custom endpoint fixes, MiniMax anthropic-compat | C provider modules need API/param sync | P1 |
| 2 | U02 | Agent loop changes | Retry buffer/fallback status, cross-provider fallback reasoning_content padding, credential pool isolation on fallback, Codex null output recovery | C agent_loop.c, retry_utils.c need sync | P1 |
| 3 | U03 | Gateway platform updates | Discord thread backfill, Windows gateway drain, Telegram heartbeat edits, platform hardening, webhook changes | C gateway platforms need sync (19 adapters) | P1 |
| 4 | U04 | Tool schema drift | Patch tool new_string unescape, TIRITH tar safety, terminal tool CI nudge, voice mode container phrasing, tool config changes | C tools need schema/behavior sync | P1 |
| 5 | U05 | MCP updates | TLS client certificates (mTLS), MCP catalog with interactive picker, SSE improvements | C MCP module needs mTLS support | P1 |
| 6 | U06 | Security/auth overhaul | Dashboard auth (OAuth PKCE, cookie, login page), API server key enforcement, CIDR allowlisting for msgraph, SSRF URL checks | C security modules need audit | P1 |
| 7 | U07 | Test suite & CI | ~17k tests, CI matrix split, flaky test hardening, test isolation, Docker CI | C: 239 tests vs ~17k — order-of-magnitude gap | P1 |

## Resolved (v24 and prior)

See vault/achievements.md for full history. Major resolved phases:
- D01-D12: Tool depth gaps (all resolved)
- X01-X05: Test gaps (all resolved)
- M03-M07: Yuanbao tools (all ported, compiled, verified)
- CI/Infra: GitHub Actions workflow gaps (all existed, were stale claims)
- Stale claim cleanup: ~35 stale battleship entries removed

## Summary

| Sector | Count | Priority |
|--------|-------|----------|
| S0: Form-vs-Function | 5 | P0 |
| S1: Pipeline & Integration | 5 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| S4: Upstream Drift (NEW) | 7 | P1 |
| **TOTAL** | **33** | **P0:5, P1:16, P2:6** |
