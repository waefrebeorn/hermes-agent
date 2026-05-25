# Hermes C — Overnight Navigation Map (v30 — 2026-05-24)

## State Verified (2026-05-24)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
| Suite | 239/0/0 (202 files) | ✅ |
| Tools registered | 83 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
| Gap count | **228** (battleship-v8) | C19 closed — cron.scheduler_poll_interval config |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v29 (previous session)

- **G04-G07, G02 closed**: DingTalk/WeCom/SMS/Mattermost/QQBot inbound polling — all wired with webhook queues and server poll threads
- **T01 closed**: DingTalk gateway test (16 tests, suite 239)
- **Phase 22 stale cleanup**: 4 battleship-v8 claims retired to vault (G01, D16, G10, L03)
- **C06 closed**: gateway.secret_rotation config key added
- **C07 closed**: tools.environments config key added
- **C10 closed**: plugins.memory.provider config key added
- **C12 closed**: credentials.sources config key added
- Gap count 239 → 238 after C07 closure
- Gap count 245 → 240 after stale retirement

## Resolved Since v29

| ID | Description |
|----|-------------|
| G04 | DingTalk inbound polling — OAuth2 + webhook queue + poll thread |
| G05 | WeCom inbound polling — webhook queue + poll thread |
| G06 | SMS webhook wiring — Twilio parser bridge |
| G07 | Mattermost bot-message filtering — self-ID filter |
| G02 | QQ Bot inbound polling — OneBot + Guild API |
| T01 | DingTalk gateway test (16 tests) |

## Current Priority Queue

From prestige_prompt.md v33:
1. **A02** — context_compressor.py port (1748 LOC)
2. **A03** — conversation_compression.py port (603 LOC)
3. **D17-D20** — Memory import/export/hash/compress/prioritize (150 LOC)
4. **G05** — WeCom inbound polling (80 LOC)

## Key Facts

- Battleship-v8 is the canonical gap list (252 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
