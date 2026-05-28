# Battle Map v30 — Real Parity Assessment + Upstream Drift

**v119 | Fork synced: 0 behind upstream, 2 ahead | C/.hermes/ tracked**
**Honest assessment: 31 gaps across 5 sectors.**

S4 (Upstream Drift) reworded — no longer "7583 commits behind" in git terms (fork is even), but the C code still lacks features from 7583 upstream changes that landed since the fork point.

## S0: Form-vs-Function Parity (P0)

| # | ID | Issue | C State | Python State | Priority |
|---|----|-------|---------|-------------|----------|
| 1 | F03 | Terminal resize race | SIGWINCH handler missing or unreliable | Clean resize handling | P0 |
| 2 | F04 | C can't hook Python | Standalone C, cannot import Python modules | Python is source of truth | P0 |
| 3 | F05 | Test cheating | 239 C test files vs ~17k Python tests | Full behavioral test suite | P0 |

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

## S4: Upstream Drift (P1)

The fork tracks upstream cleanly (0 behind). But the C code was forked from upstream at 2517917de. Since then, 7583 upstream commits added features C has never ported. Each topic below represents a feature family that exists in Python but has no C equivalent.

| # | ID | Topic | Upstream Changes (Since Fork) | C Impact | Priority |
|---|-----|-------|-----------------------------|----------|----------|
| 1 | U01 | Provider/API evolution | XAI retry 429, OAuth fixes, auth fallback, entitlement refresh, custom endpoints, MiniMax compat | C provider modules need API/param sync | P1 |
| 2 | U02 | Agent loop evolution | Retry buffer/fallback, cross-provider fallback, credential pool isolation, Codex null output | C agent_loop.c, retry_utils.c need sync | P1 |
| 3 | U03 | Gateway platform evolution | Discord thread backfill, Windows gateway drain, Telegram heartbeat, platform hardening | C gateway platforms need sync (19 adapters) | P1 |
| 4 | U04 | Tool schema evolution | Patch tool unescape, TIRITH tar safety, voice mode container fixes, web_crawl removal | C tools need schema/behavior sync | P1 |
| 5 | U05 | MCP evolution | TLS client certificates (mTLS), MCP catalog with picker, SSE improvements | C MCP module needs mTLS support | P1 |
| 6 | U06 | Security/auth evolution | Dashboard OAuth PKCE, API key enforcement, CIDR allowlisting, SSRF checks | C security modules need audit | P1 |
| 7 | U07 | Test suite gap | ~17k Python tests grown since fork | C: 239 tests — order-of-magnitude gap | P1 |

## Resolved (v24 and prior)

See vault/achievements.md for full history. Major resolved phases:
- D01-D12: Tool depth gaps (all resolved)
- X01-X05: Test gaps (all resolved)
- M03-M07: Yuanbao tools (all ported, compiled, verified)
- CI/Infra: GitHub Actions workflow gaps (all existed, were stale claims)
- Stale claim cleanup: ~35 stale battleship entries removed
- Phase 28: Fork synced to upstream, c-work branch preserved

## Summary

| Sector | Count | Priority |
|--------|-------|----------|
| S0: Form-vs-Function | 5 | P0 |
| S1: Pipeline & Integration | 5 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| S4: Upstream Drift | 7 | P1 |
| **TOTAL** | **31** | **P0:3, P1:16, P2:6** |
