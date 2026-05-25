# Hermes C — Overnight Navigation Map (v39 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v38 |
|--------|-------|-----------------|
| Suite | 226/0/23 (207 files) | ✅ tts_tool + terminal_tool deps fixed, discord_tool T06 added |
| Tools registered | 84 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ |
| Build | 30MB, 0 warnings | ✅ |
| Gap count | **107** (battleship-v8) | P08 config key |
| Source .c files | 154 | +1 |
| Library dirs | 59 | +1 |
| API server | 1015 LOC | 12 endpoints |

## What Changed Since v39 (2026-05-25)

### Session — S01 Cloud Metadata Endpoint Detection
- **S01 resolved**: Added metadata.goog, 100.100.100.200 (Alibaba Cloud), fd00:ec2 (AWS IPv6), ::ffff: (IPv4-mapped IPv6), and 100.64 (CGNAT) to url_is_always_blocked() pre-DNS checks

## Current Priority Queue

From prestige_prompt.md v43:
1. **D22** — Feishu doc/drive tool support (150 LOC, S7)
2. **N02** — Mixture of Agents tool (300 LOC, S20)
3. **U02** — TUI session browser with search (200 LOC, S14)
3. **T04** — Test coverage for untested modules (S12)

## Key Facts

- Battleship-v8 is the canonical gap list (103 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
