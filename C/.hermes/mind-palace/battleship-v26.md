# Battle Map v26 — Real Parity Assessment

**This is the honest assessment. 203 upstream commits unported. 276 C commits unreviewed against upstream.**

## S0: Form-vs-Function Parity (P0)
| # | ID | Issue | C State | Python State | Priority |
|---|----|-------|---------|-------------|----------|
| 1 | F01 | Single-turn-only agent | Works for one exchange, retry broken in TUI | Full multi-turn + retry | P0 |
| 2 | F02 | Setup not 1:1 | Partial env var setup, missing init flow parity | Full interactive setup wizard | P0 |
| 3 | F03 | Terminal resize race | SIGWINCH handler missing or unreliable | Clean resize handling | P0 |
| 4 | F04 | C can't "hook" Python | Standalone C, cannot import Python modules | Python is source of truth | P0 |
| 5 | F05 | Test cheating | 1,129 C test fns vs ~17k Python tests. Tests may pass without testing real behavior | Full behavioral test suite | P0 |

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
| # | ID | Upstream Change | Impact on C | Priority |
|---|----|----------------|-------------|----------|
| 1 | U01 | 203 upstream commits (NousResearch/hermes-agent) | Each may add/change features C needs | P1 |
| 2 | U02 | Tool schema drift | Python schemas may have changed since C ported | P1 |
| 3 | U03 | Agent loop changes | Retry logic, interrupt handling, budget tracking | P1 |
| 4 | U04 | Provider API changes | New params, auth flows, response formats | P1 |
| 5 | U05 | Gateway platform changes | New platforms, updated APIs, webhook formats | P1 |

## Resolved (v24 and prior)
See vault/achievements.md for full history. Major resolved phases include D01-D12 tool depth,
X01-X05 tests, M03-M07 Yuanbao tools, CI/infra, stale claim cleanup.

## Summary
| Sector | Count | Priority |
|--------|-------|----------|
| S0: Form-vs-Function | 5 | P0 |
| S1: Pipeline & Integration | 5 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| S4: Upstream Drift | 5 | P1 |
| **TOTAL** | **25** | P0:5, P1:14, P2:6 |
