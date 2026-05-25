# Hermes C — Overnight Navigation Map (v29 — 2026-05-24)

## State Verified (2026-05-24)

| Metric | Value | Change from v22 |
|--------|-------|-----------------|
| Suite | 238/0/0 (202 files) | ✅ |
| Tools registered | 85 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
| Gap count | **249** (battleship-v8) | G04 resolved — DingTalk inbound polling implemented |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v22 (previous session)

- **C03 resolved**: Added `agent.model_metadata` config key to agent_config_t struct
- **Phase 9 stale cleanup**: 14 battleship-v8 claims retired to vault (S05, B05, B06, B03, B10, C01-C05, C16, D04, D17-D20, P01, R04)
- **S09/S10/S11 resolved**: Plugin vtable stubs replaced with safe fallbacks (get_by_hash, compress_old, get_prioritized)
- Gap count 312 → 255 after stale retirement + stub closure

## Resolved Since v22

| ID | Description |
|----|-------------|
| C03 | agent.model_metadata config key |
| S05 | /curator run — had real llm_background_review (stale claim) |
| B05 | Gateway crash on malformed callback (stale — null checks exist) |
| B06 | Memory leak in db_list_with_meta (stale — code is clean) |
| B03 | WSL path translation (done previous session) |
| B10 | Process health check (done previous session) |
| C01-C05,C16 | Config keys all had YAML readers already |
| D04 | Insights empty-state (done previous session) |
| D17-D20 | File backend functions all had real implementations |
| P01 | Anthropic ephemeral cache headers (done previous session) |
|| R04 | HomeAssistant poll reset (done previous session) |
|| S09 | plugin_get_by_hash — safe fallback (NULL→return false) |
|| S10 | plugin_compress_old — safe fallback (NULL→return 0) |
|| S11 | plugin_get_prioritized — safe fallback (NULL→return NULL) |

## Current Priority Queue

From prestige_prompt.md v33:
1. **D16** — Plugin memory provider interface (280 LOC)
2. **G01** — Home Assistant conversation loop (200 LOC)
3. **G04** — DingTalk inbound polling (80 LOC)
4. **G05** — WeCom inbound polling (80 LOC)

## Key Facts

- Battleship-v8 is the canonical gap list (252 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
