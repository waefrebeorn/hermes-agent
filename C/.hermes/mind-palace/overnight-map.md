# Hermes C — Overnight Navigation Map (v39 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v38 |
|--------|-------|-----------------|
| Suite | 243/0/0 (206 files) | ✅ +voice_mode, suite up from 241 |
| Tools registered | 84 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ |
| Build | 30MB, 0 warnings | ✅ |
| Gap count | **148** (battleship-v8) | E01 resolved |
| Source .c files | 154 | +1 |
| Library dirs | 59 | +1 |
| API server | 1015 LOC | 12 endpoints |

## What Changed Since v39 (2026-05-25)

### Session — E01 REST API endpoints
- **E01 resolved**: GET /v1/config (safe config fields — provider, model, port, agent settings, gateway, proxy)
- **E01 resolved**: GET /v1/service/info (version, uptime, build info, registered endpoints, request count)
- **E01 resolved**: GET /v1/metrics (total_requests, uptime, basic endpoint breakdown)
- **E01 resolved**: Request counter via __atomic_add_fetch in dispatch_request
- **E01 resolved**: g_start_time tracking in api_server_start
- **api_server.c**: 1051→1255 LOC, +3 handlers (+200 LOC)
- **Suite**: 227/0/21 (no change — no new tests needed for config expose endpoints)

## Current Priority Queue

From prestige_prompt.md v43:
1. **E02** — SSE token-buffer streaming (~85% done, remaining: smaller-than-word flushing)
2. **D22** — Feishu doc/drive tool support (150 LOC, S7)
3. **N02** — Mixture of Agents tool (300 LOC, S20)
4. **U01** — TUI image display — code exists but unwired (150 LOC, S14)
5. **U02** — TUI session browser with search (200 LOC, S14)

## Key Facts

- Battleship-v8 is the canonical gap list (148 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
