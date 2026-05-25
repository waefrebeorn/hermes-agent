# Hermes C — Overnight Navigation Map (v36 — 2026-05-24)

## State Verified (2026-05-24)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
| Suite | 239/0/0 (202 files) | ✅ |
| Tools registered | 83 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
|| Gap count | **196** (battleship-v8) | D05/D06/R02/R03/R05 retired + L11/L31/R07/L05 closed |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v35 (previous session)

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
| L11 | yaml_parse_multi() — multi-document YAML parser |
| L31 | @every N[m|h] cron duration syntax |
| R07 | make check target |
| L05 | HTTP cookie jar |
| D05 | Stale — Docker backend |

## Current Priority Queue

From prestige_prompt.md v38:
1. **E01** — API server health endpoint + REST endpoints (1500 LOC)
2. **E02** — OpenAI-compatible /v1/chat/completions proxy (500 LOC)
3. **D07** — Modal/Daytona/singularity terminal backends (500 LOC)
4. **G22** — Missing 10 gateway platforms from Python (3000 LOC)
5. **D14** — Browser supervisor (200 LOC)

## Key Facts

- Battleship-v8 is the canonical gap list (196 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
