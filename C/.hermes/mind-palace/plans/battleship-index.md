# Battleship Index — Hermes C Translation

**Fresh survey — DA v15 (2026-05-24). v4 roadmap, ~270 gaps.**

Read this first, then dive into sectors via `plans/battleship-v4.md`.

## Reality (DA v15 — Fresh Code Survey)

| | Sector | Gaps | Priority | Verdict |
|-|--------|------|----------|---------|
| A | Core | ~4 | P0 | 75% done |
| B | Agent | ~44 | P0 | 57% (44/77 .c files) |
| C | CLI | ~80 | P0 | 9% (8/88 .c files) |
| D | Tools | ~20 | P1 | Most tools real, feature gaps |
| E | Gateway | ~12 | P1 | 61% (19/31 platform modules) |
| F | MCP | ~2 | P1 | Mostly done |
| G | ACP | ~4 | P1 | 56% |
| H | Cron | 0 | — | **100%** ✅ |
| I | TUI | ~6 | P1 | 25% |
| J | Plugins | ~6 | P1 | 63% |
| K | Providers | ~19 | P1 | 32% (9/28 plugins) |
| L | Config | ~10 | P1 | ~75% |
| M | Libraries | ~5 | P2 | 57 lib dirs |
| N | Build/Doc | ~5 | P1 | Mixed |
| O | Upstream | ~2 | P1 | Ongoing |
| P | Security | ~7 | P1 | 70% |
| Q | CLI Depth | ~10 | P0 | Major — 197/237 cmd stubs |
| R | Provider Quirks | ~10 | P1 | 60% |
| S | Stubs | 1 | P1 | Browser CDP tool not wired |
| T | Tests | ~20 | P1 | 173 files, timeout at 120s |
| U | CI/CD | ~5 | P2 | Basic infra |
| | **Total** | **~270** | — | **Fresh count, reset from old 500** |

## Key Files

| Purpose | Path |
|---------|------|
| Full roadmap (v4) | `.hermes/mind-palace/plans/battleship-v4.md` |
| DA v15 essay | `.hermes/mind-palace/da-audit-v15.md` |
| Priority queue | `.hermes/mind-palace/prestige_prompt.md` |
| Session tracking | `.hermes/mind-palace/state.md` |
| Bug log | `.hermes/vault/bug-bounty.md` |
| Achievements | `.hermes/vault/achievements.md` |

## Quick Stats

- **Suite:** ~213/0/0 — 173 test files (cannot complete in 120s)
- **Source:** 44 agent .c, 31 tool .c, 19 gateway .c, 5 acp .c, 8 cli .c, 10 plugin .c — ~76K LOC
- **Tools:** ~83 registry_register calls across 31 init functions
- **Plugins:** 10 .c source files (0 .so built)
- **Gateway:** 19 platforms (of 31 Python modules)
- **CLI:** 8 .c files (of 88 Python modules). 237 cmd_ functions, ~40 real, ~197 stubs
- **Providers:** 9 native C (of 28 Python plugin providers)
- **Libraries:** 57 lib/ directories
- **True stubs:** 1 (browser_cdp tool not wired to real CDP handler)
- **Binary:** 29MB ELF, 0 errors, 0 warnings

## Active Gaps

| Priority | Coordinate | What | Why Now |
|----------|-----------|------|---------|
| P0 | A01-A03, B01-B44 | Agent modules — 35+ unported | Core capability |
| P0 | C01-C80, Q01-Q10 | CLI depth — 80 modules, 197 stub cmds | Biggest gap |
| P1 | D07 | Browser CDP dead code | 300+ lines of real code not wired |
| P1 | E01 | api_server gateway | Missing platform |
| P1 | K01-K21 | Provider plugins (19 missing) | User-facing |
| P1 | T01-T20 | Test infrastructure | Suite timeout, coverage gaps |

## DA v15 Key Findings

**What changed from old battleship-v3 (500 gaps at 34%):**
- Old count was from DA v11 (May 23) when suite was 154/0/0
- Current reality is significantly ahead: 213/0/0, 9 real providers, 0 stubs except CDP
- Fresh survey found ~270 real, verifiable gaps — not 332
- Biggest delta: CLI (197 stub commands is a Major gap but counted as feature gaps not 80 separate modules)
- Many agent "missing" modules are partially covered or low-priority
