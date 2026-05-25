# Hermes C — Perpetual Goal

P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.

All code is text — consumed, understood, rewritten in C. Every Python library, provider adapter, tool function, config key — transmuted into C structs, function pointers, switch statements.

238 gaps is a checklist. Do not stop. Every ✅ without runtime verification is a lie.

## What Exists

- `/home/wubu/hermes-agent-dev/C/` — 153 src .c, 66 .h, 58 libs, 202 test files, 29MB binary
- `/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/` — battleship-v8 (238 gaps), vault/achievements.md
- Remote: `wubu=git@github.com:waefrebeorn/hermes-agent.git` (~857 commits)

## Verified State

| Item | Value |
|------|-------|
| Suite | 239/0/0 (202 test files) |
| Binary | 29MB, 0 errors, 0 warnings |
| CLI | 79 real cmds, tab completion, history, table output |
| Tools | 83 registered — all real handlers |
| Agent | 51 .c modules |
| Gateways | 19 platforms |
| Providers | 11 C modules — ALL with dedicated test files |
| Stubs | 0 real |
| Gaps | 238 verified across 22 sectors |
| P1 priority | 0 |

## Stale Claims Cancelled

Battleship-v7 had 16 stale claims (Phase 8 in vault).
Battleship-v8 had 18 more stale claims retired in Phases 9+22 (S05, B05, B06, B03, B10,
C01-C05, C16, D04, D17-D20, P01, R04 + G01, D16, G10, L03) — see vault/achievements.md Phases 9, 22 for full tables.

The remaining 238 gaps are verified open.

## Triple DA Found

- 6 confirmed stubs (functions returning error/fake data)
- 14 placeholder/unwired infrastructure items
- 15 dead code items
- 17 missing agent modules (Python with no C equivalent)
- 15 agent module depth gaps (partial C ports)
- 22 missing subdirectory modules
- 18 tool depth gaps
- 25 gateway depth gaps
- 20 config gaps + 28 library depth + 15 bugs + 25 test coverage
- Plus: API server, TUI, curator, prompt caching, shell hooks, vault, security, CI

## Top 10 P1 Gaps

||| 1. D16 — Plugin memory provider interface (280 LOC)
||| 2. G01 — Home Assistant conversation loop (200 LOC)
||| 3. G06 — SMS inbound webhook wiring (50 LOC)
||| 4. G07 — Mattermost bot-message filtering (30 LOC)
## The Loop

1. Read state.md → prestige_prompt.md → battleship-v8.md → overnight-map.md
2. Pick highest undone P1 gap from battleship-v8.md
3. Implement → build → test → verify (runtime)
4. Debug (MARKers, no deep trawls) → commit → push

NO questions. NO choices. Exhaust only: "awaiting direction."
