# Battle Map v25 — Real Parity Assessment

**This is not "gaps we found." This is "what it takes to be a real Hermes Agent."**

The v24 "0 gaps" was counting only self-identified stub-hunt gaps. The real gap is C-vs-Python feature parity across the entire agent — plumbing, UX, test integrity, and behavioral equivalence.

## S0: Form-vs-Function Parity (P0)
| # | ID | Issue | C State | Python State | Priority |
|---|----|-------|---------|-------------|----------|
| 1 | F01 | Single-turn-only agent | Works for one exchange, retry mechanism broken in TUI | Full multi-turn + retry | P0 |
| 2 | F02 | Setup not 1:1 | Partial env var setup, missing `hermes init` flow parity | Full interactive setup wizard | P0 |
| 3 | F03 | Terminal resize race | SIGWINCH handler missing or unreliable | Clean resize handling | P0 |
| 4 | F04 | C can't "hook" Python | C codebase is standalone — cannot import/use Python modules | Python codebase is the source | P0 |
| 5 | F05 | Test cheating | Tests pass but may test wrong things or skip real behavior | ~17k tests across ~900 files | P0 |

## S1: Pipeline & Integration (P1)
| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | P01 | Retry mechanism broken in TUI | Agent fails on retry in terminal UI context | P1 |
| 2 | P02 | Plumbing issues | Integration between components has edge cases | P1 |
| 3 | P03 | Linkage cheating | Dependency wiring may cut corners | P1 |
| 4 | P04 | TUI bugs | Terminal UI has display/input bugs | P1 |
| 5 | P05 | Usage bugs | General behavioral bugs in normal use | P1 |

## S2: Cross-Comparison & Auditing (P1)
| # | ID | Issue | Details | Priority |
|---|----|-------|---------|----------|
| 1 | A01 | Full AST-level cross comparison | Every Python function vs C equivalent | P1 |
| 2 | A02 | Test suite recreation | 17k Python tests → C equivalents | P1 |
| 3 | A03 | Behavioral parity audit | Same input → same output for every tool | P1 |
| 4 | A04 | Schema parity audit | Tool JSON schemas match Python exactly | P1 |

## S3: Product-Level Features (P2)
| # | ID | Feature | Details | Priority |
|---|----|---------|---------|----------|
| 1 | Q01 | Multi-turn conversation | Full back-and-forth with memory | P2 |
| 2 | Q02 | Session persistence | Full SQLite session save/restore | P2 |
| 3 | Q03 | Plugin system | Dynamic .so loading matching Python plugin model | P2 |
| 4 | Q04 | Skin engine parity | All Python skins available | P2 |
| 5 | Q05 | Gateway parity | All 19+ platforms fully tested | P2 |
| 6 | Q06 | Provider parity | All providers handling all modes (stream, thinking, caching) | P2 |

## Resolved (v24 and prior)
See vault/achievements.md for full history. Major resolved phases:
- Phase Alpha: Foundation (85 tools, CLI, gateway, providers)
- Phase Beta-Epsilon: CLI usability, display parity, core infra
- Phase Zeta: Tool features (D01-D12 depth)
- Phase Theta-Iota: Yuanbao sticker/group/DM tools
- CI/Infra: Docker, CI workflows
- Test Expansion: X01-X05 edge cases

## Summary
| Sector | Count | Priority |
|--------|-------|----------|
| S0: Form-vs-Function | 5 | P0 |
| S1: Pipeline & Integration | 5 | P1 |
| S2: Cross-Comparison | 4 | P1 |
| S3: Product Features | 6 | P2 |
| **TOTAL** | **20** | P0:5, P1:9, P2:6 |
