# Hermes C — Overnight Navigation Map (v39 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v38 |
|--------|-------|-----------------|
| Suite | 243/0/0 (206 files) | ✅ +voice_mode, suite up from 241 |
| Tools registered | 84 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ |
| Build | 30MB, 0 warnings | ✅ |
| Gap count | **156** (battleship-v8) | L06+L07 resolved |
| Source .c files | 154 | +1 |
| Library dirs | 59 | +1 |
| API server | 1015 LOC | 12 endpoints |

## What Changed Since v38 (2026-05-25)

### Session — Library Depth L06+L07
- **L06 resolved**: Configurable max_redirects via http_client_set_max_redirects()
- **L07 resolved**: gzip/deflate auto-decompression via zlib, enabled by http_client_set_decompress()
- **-lz added** to LDFLAGS and test_runner.sh for libhttp zlib linkage
- **13 HTTP tests** — setter API, redirect config, decompression flags
- **Suite**: 223/0/21 (HTTP test now compiles and passes, was skipped)

## Current Priority Queue

From prestige_prompt.md v42:
1. **E01** — REST API endpoints ~40% done (capabilities, tools, health/detailed, agent/status done)
2. **E02** — SSE streaming added (~60% done), remaining: token-by-token streaming
3. **E03** — Session CRUD via HTTP (300 LOC, next priority in S13)
4. **D22** — Feishu doc/drive tool support (150 LOC, S7)
5. **N02** — Mixture of Agents tool (300 LOC, S20)

## Key Facts

- Battleship-v8 is the canonical gap list (156 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
