# Battleship Index — Hermes C Translation

**Quick-reference dashboard.** Read this first, then dive into sectors via `plans/battleship-v2.md` (v3, DA v11 — 500 gaps).

## Reality (DA v11 — 2026-05-23, stub hunt + CI audit)

| | Sector | Gaps | Done | % | Verdict |
|-|--------|------|------|---|---------|
| A | Core | 16 | 12 | 75% | Solid |
| B | Agent | 115 | 30 | **26%** | +6 splits from refactoring |
| C | CLI | 95 | 13 | **14%** | +5 depth splits |
| D | Tools | 92 | 32 | **35%** | +6 (stub remediation targets) |
| E | Gateway | 64 | 22 | 34% | +3 test infra |
| F | MCP | 11 | 2 | 18% | — |
| G | ACP | 9 | 1 | **11%** | — |
| H | Cron | 3 | 3 | **100%** | — |
| I | TUI | 8 | 1 | 12% | — |
| J | Plugins | 26 | 10 | 38% | +4 (verification + docs) |
| L | Config | 6 | 4 | 67% | — |
| N | Build/Doc | 11 | 9 | 82% | — |
| O | Upstream | 3 | 0 | **0%** | — |
| P | Security | 10 | 6 | 60% | — |
| Q | Cross-cut | 5 | 5 | **100%** | — |
| R | Provider quirks | 18 | 11 | 61% | — |
| **S** | **Stubs** | **10** | **0** | **0%** | **NEW** — computer_use, browser CDP, image_gen, TUI |
| **T** | **Tests** | **12** | **0** | **0%** | **NEW** — gateways, TUI, MCP, fuzz, leak, thread |
| **U** | **CI/CD** | **10** | **0** | **0%** | **NEW** — Docker, ASan, cross-compile, release |
| | **Total** | **~500** | **~161** | **32%** | **339 gaps remaining** |

## Key Files

| Purpose | Path |
|---------|------|
| Full roadmap (v3) | `.hermes/mind-palace/plans/battleship-v2.md` |
| DA v11 essay | `.hermes/mind-palace/da-audit-v11-500-goals.md` |
| Priority queue | `.hermes/mind-palace/prestige_prompt.md` |
| Session tracking | `.hermes/mind-palace/state.md` |
| Bug log (16 bugs) | `.hermes/vault/bug-bounty.md` |

## Quick Stats

- **Suite:** 154/0/0 — 117 test files, ~573 assertions
- **Source:** 115 .c + 29 .h = 144 files, 75.5K LOC
- **Tools:** 68 registered (up from 28 reported previously)
- **Plugins:** 10 .so
- **Gateway:** 19 platforms
- **CLI:** ~148 commands
- **Stubs found:** 4 (computer_use 100%, browser CDP 5/11, image_gen URL fabricator, TUI session placeholder)
- **Dockerfile bug:** Fixed (Wrong working directory in `RUN make`)

## Active Gaps

| Priority | Coordinate | What | Why Now |
|----------|-----------|------|---------|
| P0 | S01-S03 | Stub: computer_use real backend | 5 registered features returning errors |
| P0 | D75-D79 | New upstream ports — computer_use backends | Unblocks key features |
| P0 | U01-U02 | CI must pass + Docker build fix | Gate to merge |
| P1 | D87-D88 | Stub: CDP browser backend | 5 browser tools non-functional |
| P1 | S07 | Stub: image_gen real backend (Fal AI) | Plugin produces fake URLs |
| P1 | S10 | TUI session browser DB-backed | Placeholder UX |
| P1 | T01-T02 | Gateway + CLI test coverage | Catch regressions |
| P1 | U04 | ASan CI job | Memory safety |
| P2 | M02-M05, M09 | Test coverage — cron_cli, jobs, scheduler | Catch bugs |

## DA Audit Notes (v11)

**Key findings:**
- 4 verified stubs: computer_use (100%), browser CDP (5 tools), image_gen (fake URLs), TUI sessions (placeholder)
- Dockerfile was broken (wrong working directory) — CI would have failed on first run
- "32 .a libraries" claim is misleading — .a files don't exist, libs compile directly to .o
- 68 tools registered (not 28 as previously reported) — tools are at ~95% parity
- CI workflows had truncated build logs (tail -20) — fixed to show full output
- nix.yml failures are pre-existing Python/Node infrastructure issues — unrelated to C
