# Hermes C — Overnight Navigation Map (v37 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
| Suite | 239/0/0 (202 files) | ✅ |
| Tools registered | 83 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
|| Gap count | **193** (battleship-v8) | E01 ~40% done |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v36 (2026-05-25)

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
