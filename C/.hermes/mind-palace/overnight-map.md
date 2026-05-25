# Hermes C — Overnight Navigation Map (v39 — 2026-05-25)

## State Verified (2026-05-25)

| Metric | Value | Change from v38 |
|--------|-------|-----------------|
| Suite | 243/0/0 (206 files) | ✅ +voice_mode, suite up from 241 |
| Tools registered | 84 | ✅ |
| CLI commands | 79 | ✅ |
| Real stubs | 0 (all resolved) | ✅ |
| Build | 30MB, 0 warnings | ✅ |
| Gap count | **152** (battleship-v8) | L12 resolved |
| Source .c files | 154 | +1 |
| Library dirs | 59 | +1 |
| API server | 1015 LOC | 12 endpoints |

## What Changed Since v38 (2026-05-25)

### Session — AES-256-GCM Encryption
- **L12 resolved**: AES-256-GCM encrypt/decrypt via OpenSSL EVP
- **L30 resolved**: libmcp streaming — mcp_server_call_tool_stream, transport_read_any, callback-based chunk delivery
- **L02 resolved**: libhttp connection pooling — pool_acquire/release, http_client_set_pool, LRU eviction
- **L04 resolved**: libhttp multipart form data — builder API, boundary generation, http_post_multipart
- crypto_aes_encrypt(): random IV, GCM tag, SHA-256 key derivation for short keys
- crypto_aes_decrypt(): authentication tag verification, NULL on tamper
- **6 crypto tests**: roundtrip, wrong key auth fail, short key, null safety
- **Suite**: 223/0/21 (crypto test already registered, +14→20 tests)

## Current Priority Queue

From prestige_prompt.md v42:
1. **E01** — REST API endpoints ~40% done (capabilities, tools, health/detailed, agent/status done)
2. **E02** — SSE streaming added (~60% done), remaining: token-by-token streaming
3. **E03** — Session CRUD via HTTP (300 LOC, next priority in S13)
4. **D22** — Feishu doc/drive tool support (150 LOC, S7)
5. **N02** — Mixture of Agents tool (300 LOC, S20)

## Key Facts

- Battleship-v8 is the canonical gap list (152 gaps)
- Vault/achievements.md is the archive — all completed + retired stale claims
- Python upstream: 77 agent modules → 50 C; 88+ tools → 43 C; 31 gateways → 19 C
- P1 count is 0
