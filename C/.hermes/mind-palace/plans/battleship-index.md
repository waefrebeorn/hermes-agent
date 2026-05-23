# Battleship Index — Hermes C Translation

**Quick-reference dashboard.** Read this first, then dive into sectors via `plans/battleship-v2.md` (v2, DA v10 — 468 gaps).

## Reality (DA v10 — 2026-05-23, upstream-synced)

| | Sector | Gaps | Done | % | Verdict |
|-|--------|------|------|---|---------|
| A | Core | 16 | 12 | 75% | Solid |
| B | Agent | 109 | 30 | **28%** | **Biggest gap** — +2 from upstream (secret_sources) |
| C | CLI | 90 | 13 | **14%** | +1 from upstream (secrets_cli) |
| D | Tools | 86 | 32 | **37%** | +12 from upstream (computer_use, fal, ssh, etc.) |
| E | Gateway | 61 | 22 | 36% | — |
| F | MCP | 11 | 2 | 18% | — |
| G | ACP | 9 | 1 | **11%** | — |
| H | Cron | 3 | 3 | **100%** | — |
| I | TUI | 8 | 1 | 12% | — |
| J | Plugins | 22 | 10 | 45% | +5 from upstream (fal, discord, openviking) |
| L | Config | 6 | 4 | 67% | — |
| N | Build/Doc | 11 | 9 | 82% | +1 (parallel test runner) |
| O | Upstream | 3 | 0 | **0%** | 183 commits merged — now synced |
| P | Security | 10 | 6 | 60% | — |
| Q | Cross-cut | 5 | 5 | **100%** | — |
| R | Provider quirks | 18 | 11 | 61% | — |
| | **Total** | **~468** | **~161** | **34%** | **307 gaps remaining** |

## Key Files

| Purpose | Path |
|---------|------|
| Full roadmap (v2) | `.hermes/mind-palace/plans/battleship-v2.md` |
| Priority queue | `.hermes/mind-palace/prestige_prompt.md` |
| Session tracking | `.hermes/mind-palace/state.md` |
| Bug log (16 bugs) | `.hermes/vault/bug-bounty.md` |
| All milestones | `.hermes/vault/achievements.md` |
| PBS credits ($69.32) | `.hermes/vault/credits.md` |
| Legacy plans archive | `.hermes/vault/legacy-plans-archive.md` |
## Quick Stats

- **Suite:** 154/0/0 — 117 test files, ~573 assertions
- **Source:** 115 .c + 29 .h = 144 files, 75.5K LOC
- **Libraries:** 32 .a (100%)
- **Plugins:** 10 .so
- **Gateway:** 19 platforms
- **CLI:** ~148 commands
- **Commits:** 400 C-specific (0 behind upstream)
- **Bugs found:** 16 (6 critical, 10 high)
- **Project age:** ~2-3 days AI work (May 22-23, 2026)

## Active Gaps

| Priority | Coordinate | What | Why Now |
|----------|-----------|------|---------|
| P0 | D75-D79, D82, D84 | New upstream ports — computer_use backends (5 files), transcription, SSH env | Unblocks key features |
| P1 | M02-M05, M09 | Test coverage — cron_cli, jobs, scheduler, tool handler files (11 untested) | Catch bugs |
| P1 | B108-B109 | Upstream secrets subsystem (secret_sources/bitwarden) | Credential management |
| P2 | R12-R18 | Provider API quirks (7 remaining) | Deep coverage |
| P2 | C05, C10-C90 | CLI depth — secrets CLI, setup, models, plugins cmd | UX polish |
| P2 | N09 | Windows build (O02) | Platform portability |

## DA Audit Notes (v9)

**Methodology:** Triple source verification — (1) git ls-files count, (2) grep for struct/function defs, (3) test file count per area.

**Findings:**
- Previous "~69% parity" was aspirational — counted structural porting not real gap coverage
- Real parity 36% is HONEST — counts every Python file without C equivalent
- B sector (Agent) is the real drag: 107 gaps, only 30 done
- C sector (CLI) has shockingly low 15% — 76 missing CLI management features
- ACP (11%) and TUI (12%) are near-zero — large architectural efforts
- The 36% is measured against origin/main (NousResearch upstream), not wubu/main

**Verification levels applied:**
- ✅ = compiles AND has test coverage OR verified against Python output
- 🔄 = compiles but missing significant functionality
- ❌ = no C equivalent exists
