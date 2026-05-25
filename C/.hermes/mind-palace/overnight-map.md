# Hermes C — Overnight Navigation Map (v34 — 2026-05-24)

## State Verified (2026-05-24)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
| Suite | 239/0/0 (202 files) | ✅ |
| Tools registered | 83 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
| Gap count | **218** (battleship-v8) | B16 retired stale |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v35 (previous session)

- **B16 retired stale**: db_list_with_meta already has OOM check at calloc via NULL guard (db.c:522)
- Gap count 219 → 218 after B16 stale retirement

## Resolved Since v35

| ID | Description |
|----|-------------|
| B16 | Stale claim retired — OOM check already present in db_list_with_meta |

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
