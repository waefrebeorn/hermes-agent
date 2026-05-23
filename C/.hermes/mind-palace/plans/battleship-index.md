# Battleship Index — Hermes C Translation

**Quick-reference dashboard.** Read this first, then dive into sectors via `plans/300-gap-roadmap-v1.md`.

## Reality (DA v9 — 2026-05-23, source-verified)

| | Sector | Gaps | Done | % | Verdict |
|-|--------|------|------|---|---------|
| A | Core | 16 | 12 | 75% | Solid — main loop, CLI, config, tools registry |
| B | Agent | 107 | 30 | **28%** | **Biggest gap** — 77 missing: compression, caching, transports, LSP |
| C | CLI | 89 | 13 | **15%** | **Massive gap** — 76 missing CLI features (setup, models, plugins, gateway) |
| D | Tools | 74 | 32 | 43% | Core done, browser/computer_use/video/environments missing |
| E | Gateway | 61 | 22 | 36% | 19 platforms done, plumbing (hooks/mirror/pairing) missing |
| F | MCP | 11 | 2 | 18% | MCP server + lib done, transports (Codex/Gemini/Bedrock) missing |
| G | ACP | 9 | 1 | **11%** | Bare stub — VS Code/JetBrains integration not ported |
| H | Cron | 3 | 3 | **100%** | Fully complete (8 C files) |
| I | TUI | 8 | 1 | 12% | ncurses stub far from React Ink parity |
| J | Plugins | 17 | 10 | 59% | 10 .so built, 7 missing (mem0, supermemory, model-providers, etc.) |
| L | Config | 6 | 4 | 67% | Watching + migration missing |
| N | Build/Doc | 10 | 9 | 90% | Windows build (O02) only gap |
| O | Upstream | 3 | 0 | **0%** | 183 commits behind, digest file map stale |
| P | Security | 10 | 6 | 60% | Skills guard, slash access, credential depth missing |
| Q | Cross-cut | 5 | 5 | **100%** | Token counting, secure paths, key leakage — all done |
| R | Provider quirks | 18 | 11 | 61% | 7 remaining subtle API mismatches |
| | **Total** | **~447** | **~161** | **36%** | **286 gaps remaining** |

## Quick Stats

- **Suite:** 154/0/0 — 116 test files, ~573 assertions
- **Libraries:** 30 .a (100%)
- **Plugins:** 10 .so (59%) 
- **Gateway:** 19 platforms (100%)
- **CLI:** ~148 commands (15% of Python CLI depth)
- **Commits:** 396 C-specific
- **Binary:** 9.2M dynamically linked
- **Upstream:** 183 commits behind
- **Bugs found:** 16 (6 critical, 10 high) — see `vault/bug-bounty.md`
- **Project age:** ~2-3 days of AI work (May 22-23, 2026), not months — dates in docs reflect this

## Active Gaps (P1 — next session picks first)

| Priority | Coordinate | What | Why Now |
|----------|-----------|------|---------|
| P1 | M02-M05, M09 | Test coverage — cron_cli, jobs, scheduler, tool handler files (11 untested) | Catch bugs |
| P1 | O01 | Upstream catch-up (183 commits) | Feature erosion |
| P1 | R12-R18 | Provider API quirks (7 remaining) | Deep coverage |
| P1 | C05, C10-C89 | CLI depth — autocomplete, setup, models, plugins cmd, gateway cmd | UX polish |
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

## Key Files

| Purpose | Path |
|---------|------|
| Full roadmap (447 gaps) | `.hermes/mind-palace/plans/300-gap-roadmap-v1.md` |
| Priority queue | `.hermes/mind-palace/prestige_prompt.md` |
| Session tracking | `.hermes/mind-palace/state.md` |
| Bug log (16 bugs) | `.hermes/vault/bug-bounty.md` |
| All milestones | `.hermes/vault/achievements.md` |
| PBS credits ($69.32) | `.hermes/vault/credits.md` |
| Legacy plans archive | `.hermes/vault/legacy-plans-archive.md` |
