# Hermes C — Perpetual Goal

P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.

All code is text — consumed, understood, rewritten in C. Every Python library, provider adapter, tool function, config key — transmuted into C structs, function pointers, switch statements.

148 gaps is a checklist. Do not stop. Every ✅ without runtime verification is a lie.

## What Exists

- `/home/wubu/hermes-agent-dev/C/` — 154 src .c, 66 .h, 59 libs, 206 test files, 30MB binary
- `/home/wubu/hermes-agent-dev/C/.hermes/mind-palace/` — battleship-v8 (148 gaps), vault/achievements.md
- Remote: `wubu=git@github.com:waefrebeorn/hermes-agent.git` (~858 commits)

## Verified State

| Item | Value |
|------|-------|
| Suite | 243/0/0 (206 test files) |
| Binary | 30MB, 0 errors, 0 warnings |
| CLI | 79 real cmds, tab completion, history, table output |
| Tools | 84 registered — all real handlers |
| Agent | 51 .c modules |
| Gateways | 19 platforms |
| Providers | 11 C modules — ALL with dedicated test files |
| Stubs | 0 real |
|| Gaps | **148** verified across 22 sectors |
| P1 priority | 0 |

## Stale Claims Cancelled

Battleship-v7 had 16 stale claims (Phase 8 in vault).
Battleship-v8 had 19 stale claims retired in Phases 9+22+27 (N01, S05, B05, B06, B03, B10,
C01-C05, C16, D04, D17-D20, P01, R04 + G01, D16, G10, L03, D23, C17, I01, G22)
— see vault/achievements.md Phases 9, 22, 27 for full tables.

The remaining 148 gaps are verified open.

## Top Priority Gaps

From prestige_prompt.md v42:
1. **E02** — OpenAI-compatible /v1/chat/completions proxy + SSE token-buffer (100 LOC)
2. **D22** — Feishu doc/drive tool support (150 LOC)
3. **N02** — Mixture of Agents tool (300 LOC)
4. **U01** — TUI image display — code exists but unwired (150 LOC)
5. **U02** — TUI session browser with search (200 LOC)

## The Loop

1. Read state.md → prestige_prompt.md → battleship-v8.md → overnight-map.md
2. Pick highest undone gap from battleship-v8.md
3. Implement → build → test → verify (runtime)
4. Debug (MARKers, no deep trawls) → commit → push

NO questions. NO choices. Exhaust only: "awaiting direction."
