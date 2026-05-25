# Hermes C — Overnight Navigation Map (v38 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
|| Suite | 241/0/0 (203 files) | ✅ +voice_mode |
| Tools registered | 84 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
|| Gap count | **182** (battleship-v8) | E01 ~40% done |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v36 (2026-05-25)

### Session 2 (2026-05-25)
- **T03 closed**: Added clarify test coverage (8 tests, suite 241/0/0)
- **D23 retired stale**: Web search provider abstraction — web_search_registry.c exists (239+217 test)
- **C17/I01/G22 retired stale**: Checkpoint config, CI, gateway platform count
- **Doc sweep**: state/v43, prestige/v40, plan/v29, overnight/v38 update
- **Gap count**: 193 → 182 (11 resolved: 8 implemented + 3 stale)
- **Suite**: 240→241/0/0, Tools: 83→84

- **E01 ~40%**: Added REST API endpoints (capabilities, tools, health/detailed, agent/status) to api_server.c
- **E01 wired**: `hermes api-server [port]` CLI command added (like mcp-serve)
- **E02 ~60%**: SSE streaming for POST /v1/chat/completions via ?stream=true query param
- **api_server.c** grew 457 → ~850 lines (+393 across both sessions)
- 7 endpoints now available including SSE streaming
- Suite still 239/0/0, build 0 warnings

- **L05 closed**: Added HTTP cookie jar (Set-Cookie parse, Cookie build, domain/path/secure matching, wire into do_request)
- **L31 closed**: @every N[m|h] duration syntax in cron core
- **L11 closed**: Multi-document YAML parser with `---` separators
- **R07 closed**: make check target (lint + build + test)
- **D05 retired stale**: Docker backend already implemented
- **D06 retired stale**: SSH backend already implemented
- **R02/R03/R05 retired stale**: Curator (status/run/review) all implemented
- Gap count 201 → 196

## Resolved Since v41

| ID | Description |
|----|-------------|
| E02 | SSE streaming for /v1/chat/completions (?stream=true) — ~60% done |
| E01 | REST API endpoints ~40% — capabilities, tools, health/detailed, agent/status, CLI wiring |
| L11 | yaml_parse_multi() — multi-document YAML parser |
| L31 | @every N[m|h] cron duration syntax |
| R07 | make check target |
| L05 | HTTP cookie jar |
| D05 | Stale — Docker backend |

## Current Priority Queue

From prestige_prompt.md v39:
1. **E01** — REST API endpoints ~40% done (capabilities, tools, health/detailed, agent/status done)
2. **E02** — SSE streaming added (~60% done), remaining: true token-by-token streaming
3. **E03** — Session CRUD via HTTP (300 LOC, next priority in S13)
4. **D07** — Modal/Daytona/singularity terminal backends (500 LOC)
5. **G22** — Missing 10 gateway platforms from Python (3000 LOC)

## Key Facts

- Battleship-v8 is the canonical gap list (196 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
