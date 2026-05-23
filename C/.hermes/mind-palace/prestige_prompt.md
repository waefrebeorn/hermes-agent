# WuBu Hermes C — Prestige Prompt (v11 — 2026-05-23)

## Identity
1:1 C reimplementation of Python hermes-agent. 144 source files (115 .c + 29 .h), 30 library units, 68 tools, 19 gateways, 10 plugins, ~148 CLI commands. 400 C-specific commits. Synced upstream.

## Current State
| Metric | Value |
|--------|-------|
| Suite | 154/0/0 (117 tests, ~573 assertions) |
| Binary | 9.1MB dynamic ELF |
| Tools registered | 68 |
| Gateway platforms | 19 |
| Plugins .so | 10 |
| CLI commands | ~148 |
| Config keys | ~322 |
| Parity | 32% (161/500) |
| Upstream | 0 behind, 400 ahead |
| Bugs found | 16 (6 critical) |
| C LOC | 75.5K (src) |

## Priority Queue

### P0 — Stub Remediation (broken features)
| # | ID | What | Why Now |
|---|----|------|---------|
| 1 | S01-S03 | computer_use real backend | 5 registered tools return errors |
| 2 | S04-S06 | CDP browser backend | 5/11 browser tools non-functional |
| 3 | U01-U02 | CI must pass + Docker fix | Gate to merge |

### P1 — Test Infrastructure
| # | ID | What | Why Now |
|---|----|------|---------|
| 4 | T01 | Gateway per-platform integration tests | 19 platforms, 0 tests |
| 5 | T02 | CLI command coverage >80% | Currently ~60% |
| 6 | T07 | Plugin sandbox loading tests | Security boundary |
| 7 | T09 | Valgrind/ASan leak detection | Memory safety |

### P1 — Feature Depth
| # | ID | What | Why Now |
|---|----|------|---------|
| 8 | S07 | image_gen real backend (Fal AI) | Plugin produces fake URLs |
| 9 | S10 | TUI session browser | Placeholder UX |
| 10 | D75-D79 | computer_use upstream backports | New from Python sync |

### P2 — Polish
| # | ID | What | Why Now |
|---|----|------|---------|
| 11 | U04 | ASan CI job | Catch memory bugs |
| 12 | U05 | Cross-compilation matrix | Portability |
| 13 | C91-C95 | CLI depth (autocomplete, rich formatting) | UX polish |
| 14 | T08 | Fuzz testing | Input robustness |

## Key Files
- **Battleship:** `plans/battleship-v2.md` (full 500-gap roadmap)
- **DA v11:** `da-audit-v11-500-goals.md` (stub hunt, CI fix, 500 expansion)
- **State:** `state.md` (binary truth table)
- **Bugs:** `vault/bug-bounty.md` (16 bugs)
