# Battle Map v32 — Real Parity Assessment + Upstream Drift

**v123 | Fork synced: 0 behind upstream, 0 ahead | C/.hermes/ tracked**
**Honest assessment: 23 gaps across 5 sectors.** (U04 partial: patch tool \\t/\\r unescape ported from upstream 78be45860)

S4 (Upstream Drift) reworded — no longer "7583 commits behind" in git terms (fork is even), but the C code still lacks features from 7583 upstream changes that landed since the fork point.

## S0: Form-vs-Function Parity (P0)

| # | ID | Issue | C State | Python State | Priority |
|---|----|-------|---------|-------------|----------|
| 1 | F04 | C can't hook Python | Standalone C, cannot import Python modules | Python is source of truth | P0 |
| 2 | F05 | Test cheating | 239 C test files vs ~17k Python tests | Full behavioral test suite | P0 |

## S1: Pipeline & Integration (P1)

| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | P02 | Plumbing edge cases | Integration between components has bugs | P1 |
| 2 | P03 | Linkage/dependency integrity | Dependency wiring may cut corners vs Python | P1 |
| 3 | P04 | TUI display bugs | Display/input bugs in terminal UI | P1 |
| 4 | P05 | General usage bugs | Behavioral bugs in normal operation | P1 |

## S2: Cross-Comparison (P1)

| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | A01 | Full AST tool comparison | Every Python tool vs C equivalent | P1 |
| 2 | A02 | Test suite recreation | 17k Python tests → C equivalents | P1 |
| 3 | A03 | Behavioral parity | Same input → same output for all tools | P1 |
| 4 | A04 | JSON schema parity | Tool schemas must match Python exactly | P1 |

## S3: Product Features (P2)

Key: 🔲 implemented-but-unwired, 🟡 feature implemented with simpler backend

| # | ID | Feature | Details | Priority |
|---|----|---------|---------|----------|
| 1 | Q01 | Multi-turn conversation | 🟡 `agent_chat()` loop in CLI, message history maintained across turns | P2 |
| 2 | Q02 | Session persistence | 🟡 File-based JSON sessions (db.c), save/load/meta all wired | P2 |
| 3 | Q03 | Plugin system | 🟡 10 .so plugins loaded via dlopen, partial Python plugin model parity | P2 |
| 4 | Q04 | Skin engine parity | 🟡 libskin + display_set_skin + skin colors in status bar | P2 |
| 5 | Q05 | Gateway platform parity | 🟡 19 platforms, gateway_subsystem (49 tests), gateway_escape (30 tests) | P2 |
| 6 | Q06 | Provider mode parity | 🟡 stream_diag_t, cache tracking, thinking/vision flags in header | P2 |

## S4: Upstream Drift (P1)

The fork tracks upstream cleanly (0 behind). But the C code was forked from upstream at 2517917de. Since then, 7583 upstream commits added features C has never ported. Each topic below represents a feature family that exists in Python but has no C equivalent.

| # | ID | Topic | Upstream Changes (Since Fork) | C Impact | Priority |
|---|-----|-------|-----------------------------|----------|----------|
| 1 | U01 | Provider/API evolution | XAI retry 429, OAuth fixes, auth fallback, entitlement refresh, custom endpoints, MiniMax compat | C provider modules need API/param sync | P1 |
| 2 | U02 | Agent loop evolution | Retry buffer/fallback, cross-provider fallback, credential pool isolation, Codex null output | C agent_loop.c, retry_utils.c need sync | P1 |
| 3 | U03 | Gateway platform evolution | Discord thread backfill, Windows gateway drain, Telegram heartbeat, platform hardening | C gateway platforms need sync (19 adapters) | P1 |
| 4 | U04 | Tool schema evolution | Patch tool unescape (DONE), TIRITH tar safety, voice mode container fixes, web_crawl removal | C tools need schema/behavior sync | P1 |
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
| S0: Form-vs-Function | 2 | P0 |
| S1: Pipeline & Integration | 4 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| S4: Upstream Drift | 7 | P1 |
| **TOTAL** | **23** | **P0:2, P1:15, P2:6** |

## Resolved Since v32
- U04 partial: patch tool \\t/\\r unescape ported from upstream @78be45860 (suite 283/0/0, 37 patch tests pass)
