# Battle Map v33 — Real Parity Assessment

**v129 | Fork synced: 0 behind upstream, 0 ahead | Suite 283/0/0 | 85 tools | 98 CLI**
**Honest assessment: 18 gaps across 5 sectors.** (W12, W14 resolved. 17 gaps remaining.)

v33 removes stale upstream-drift predictions that never materialized (S4 U01-U06 items verified against both C and Python source — features either already exist in C or don't exist in either codebase). S4 reduced from 7 to 1 item (U07: test gap). S1 expanded to include the remaining real warnings/bugs.

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

🟡 = feature implemented with simpler backend

| # | ID | Feature | Details | Priority |
|---|----|---------|---------|----------|
| 1 | Q01 | Multi-turn conversation | 🟡 `agent_chat()` loop in CLI, message history maintained across turns | P2 |
| 2 | Q02 | Session persistence | 🟡 File-based JSON sessions (db.c), save/load/meta all wired | P2 |
| 3 | Q03 | Plugin system | 🟡 10 .so plugins loaded via dlopen, partial Python plugin model parity | P2 |
| 4 | Q04 | Skin engine parity | 🟡 libskin + display_set_skin + skin colors in status bar | P2 |
| 5 | Q05 | Gateway platform parity | 🟡 19 platforms, gateway_subsystem (49 tests), gateway_escape (30 tests) | P2 |
| 6 | Q06 | Provider mode parity | 🟡 stream_diag_t, cache tracking, thinking/vision flags in header | P2 |

## S4: Upstream Drift & Test Gap (P1)

| # | ID | Topic | Details | Priority |
|---|-----|-------|---------|----------|
| 1 | U07 | Test suite gap | ~17k Python tests grown since fork; C: 239 tests — order-of-magnitude gap | P1 |

## Summary

| Sector | Count | Priority |
|--------|-------|----------|
| S0: Form-vs-Function | 2 | P0 |
| S1: Pipeline & Integration | 4 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| S4: Upstream Drift | 1 | P1 |
| **TOTAL** | **17** | **P0:2, P1:9, P2:6** |

## Resolved Since v32

See vault/achievements.md Phase 38 for full details. Major changes:
- U01-U06 retired (stale predictions of upstream changes that never materialized)
- S1 expanded with real warning/bug findings
- Vault Phase 38 documents each stale claim with evidence
