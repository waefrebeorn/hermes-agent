# Hermes C — Overnight Navigation Map (v34 — 2026-05-24)

## State Verified (2026-05-24)

| Metric | Value | Change from v29 |
|--------|-------|-----------------|
| Suite | 239/0/0 (202 files) | ✅ |
| Tools registered | 83 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ 17 resolved |
| Build | 29MB, 0 warnings | ✅ |
| Gap count | **216** (battleship-v8) | L08 — JSON pointer queries added |
| Python modules scanned | 77 agent, 88+ tools, 31 gateways | ✅ |

## What Changed Since v35 (previous session)

- **L08 closed**: Added `json_pointer_get()` — RFC 6901 JSON Pointer query in libjson (60 LOC, 11 tests)
- **D13 retired stale**: Browser dialog handler already fully implemented
- Gap count 218 → 216

## Resolved Since v35

| ID | Description |
|----|-------------|
| L08 | `json_pointer_get()` — JSON Pointer queries added to libjson |
| D13 | Stale claim retired — browser dialog handler already exists |

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
