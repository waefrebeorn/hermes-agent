# Hermes C — State Dashboard (v50 — 2026-05-25)

## Build Metrics (code-verified)

| Metric | Value | As Of |
|--------|-------|-------|
|| Suite | **228/0/21** — 207 test files | 2026-05-25 |
| Binary | **30MB ELF**, 0 warnings | 2026-05-25 |
| Source `.c` files | **154** | 2026-05-25 |
| Headers | **66** | 2026-05-25 |
| Library directories | **59** | 2026-05-25 |
| Tools registered | **84** (all real handlers) | 2026-05-25 |
| CLI commands | **79** | 2026-05-25 |
| Gateway platforms | **19** | 2026-05-25 |
| Agent `.c` modules | **51** | 2026-05-25 |
| Provider modules | **11** (all with tests) | 2026-05-25 |
| Provider test files | **11** | 2026-05-25 |
| C plugins | **10** | 2026-05-25 |
| Git commits | **858+** | 2026-05-25 |

## Battleship Status

- **Active:** battleship-v8 (127 verified gaps across 22 sectors)
- **Retired:** battleship-v7 (all stale claims moved to vault)
- **Vault:** achievements.md updated with all completed work and retired stale claims

## Gap Summary (from battleship-v8)

| Sector | Name | Gaps | P1 | P2 | P3 |
|--------|------|------|-----|-----|------|
| S1 | Stubs | 0 | 0 | 0 | 0 |
| S2 | Placeholder/Unwired | 11 | 0 | 1 | 10 |
| S3 | Dead Code | 12 | 0 | 2 | 10 |
| S4 | Missing Agent Modules | 12 | 0 | 8 | 4 |
| S5 | Agent Module Depth | 15 | 0 | 12 | 3 |
| S6 | Missing Subdirectory | 22 | 0 | 12 | 10 |
| S7 | Tool Depth | 0 | 0 | 0 | 0 |
| S8 | Gateway Depth | 15 | 0 | 13 | 2 |
| S9 | Config/Environment | 0 | 0 | 0 | 0 |
||| S10 | Library Depth | 6 | 0 | 2 | 4 |
|| S11 | Bug Fixes | 2 | 0 | 1 | 1 |
|| S12 | Test Coverage | 2 | 0 | 2 | 0 |
| S13 | API Server | 0 | 0 | 0 | 0 |
| S14 | TUI Depth | 1 | 0 | 0 | 1 |
| S15 | Curator | 0 | 0 | 0 | 0 |
| S16 | Prompt Caching | 0 | 0 | 0 | 0 |
| S17 | Shell Hooks | 0 | 0 | 0 | 0 |
| S18 | Vault Encryption | 3 | 0 | 2 | 1 |
| S19 | Security | 2 | 0 | 0 | 2 |
| S20 | New Features | 8 | 0 | 0 | 8 |
| S21 | Refactoring | 9 | 0 | 2 | 7 |
| S22 | CI/Integration | 7 | 0 | 6 | 1 |
||| **Total** | **127** | **0** | **54** | **73** |

## Python Upstream Parity

| Category | Python | C | Coverage |
|----------|--------|---|----------|
| Agent modules | 77 | 50 (26 direct match + 24 C-only) | ~65% coverage |
| Tool files | 88+ | 43 tool `.c` files | ~49% files |
| Gateway platforms | 31 | 19 | 61% |
| Plugins | 138 dirs (many optional-shared) | 10 C plugins | core plugins done |

## Stale Claims Retired This Session

- **N01**: Bitwarden Secrets Manager — secrets.c (330 LOC) + hermes_secrets.h (55 LOC) already implement full integration
- **D23**: Web search provider abstraction — web_search_registry.c (239 LOC + 217 test) already exists

## Recently Resolved

| ID | Description | Sector | Date |
|----|-------------|--------|------|
| T24 | voice_mode.c — 20 tests (state mgmt, config, args) | S12 | 2026-05-25 |
| D14 | Browser supervisor — cdp_supervisor_ping() with Browser.getVersion | S7 | 2026-05-25 |
| D15 | Camofox session persistence — save/load/delete browser state | S7 | 2026-05-25 |
| D10 | Computer use backend registry — register/list/clear backends, CU_BACKEND env | S7 | 2026-05-25 |
| D11 | Vision routing — vision→som fallback with notification | S7 | 2026-05-25 |
| C08 | Config key agent.codex_runtime (auto\|codex_app_server) | S9 | 2026-05-25 |
| C11 | MoA config keys (enabled, model, strategy, workers) | S9 | 2026-05-25 |
| G20 | Weixin rich media - send_video (msg_type=4), send_file (msg_type=6) | S8 | 2026-05-25 |
| G18 | Webhook custom HMAC algorithms - SHA1, MD5, configurable algorithm dispatch | S8 | 2026-05-25 |
| H02 | Shell hook async execution - fork fire-and-forget, async flag | S17 | 2026-05-25 |
| H03 | Shell hook return value chaining - chain callback, pass output to next | S17 | 2026-05-25 |
|| H01 | Shell hook priority ordering - priority field, JSON parse, qsort | S17 | 2026-05-25 |
|| B11 | Shell hook shutdown - unregister mismatch (shell_hook_callback v chain_callback) | S11 | 2026-05-25 |
|| S05 | Process argument injection detection - metacharacter, flag, null byte scanning | S19 | 2026-05-25 |
|| S04 | File path traversal depth - realpath resolution, URL-encoded .., null byte, symlink detection | S19 | 2026-05-25 |
|| P04 | Multi-turn cache optimization — stale, already implemented via g_marked_count + start_idx skip | S16 | 2026-05-25 |
|| S03 | Credential leak detection in tool outputs — tirith_has_credential_leak(), API key, JWT, AWS, DB connection patterns | S19 | 2026-05-25 |
|| P05 | Cache warmup on session load — cache_warmup() API for restored session messages | S16 | 2026-05-25 |
| P06 | Per-provider cache config - provider_cache_find, prompt_cache_set_provider_config | S16 | 2026-05-25 |
| U03 | TUI skill browser - list skills from ~/.hermes/skills/ | S14 | 2026-05-25 |
| U07 | TUI cron job viewer - config, commands, guide | S14 | 2026-05-25 |
| U08 | TUI log viewer - tail agent.log, file paths | S14 | 2026-05-25 |
| U06 | TUI gateway status dashboard - RPC status, agent/DB state, platform info | S14 | 2026-05-25 |
| U04 | TUI config editor - search, get value, explain, help bar update | S14 | 2026-05-25 |
| T02 | approve.c — 34 tests for pattern matching, cache, yolo mode, URL safety | S12 | 2026-05-25 |
| U02 | TUI session browser with metadata/search - title, msg count, model, time | S14 | 2026-05-25 |
| U01 | TUI image display - wired sixel/kitty image viewer | S14 | 2026-05-25 |
| D07 | Modal terminal backend — run_command_modal() via `modal run` | S7 | 2026-05-25 |
| D08 | file_sync library — collect mkdir upload_all (14 tests) | S7 | 2026-05-25 |
| T11 | image_gen.c — 8 tests (null args, error paths, validation) | S12 | 2026-05-25 |
| T03 | clarify.c — 8 tests (null args, schema, error paths) | S12 | 2026-05-25 |
| C17 | agent.checkpoint.* — already implemented (8 fields, YAML reader) | S9 | 2026-05-25 |
| I01 | GitHub Actions CI — already exists (.github/workflows) | S22 | 2026-05-25 |
| G22 | Missing gateway platforms — both Python and C have ~19 (partially stale) | S8 | 2026-05-25 |
| D23 | Web search provider abstraction — web_search_registry.c exists | S7 | 2026-05-25 |
|| N01 | Bitwarden Secrets Manager — stale, already implemented | S20 | 2026-05-25 |
|| L06 | libhttp redirect following configurable — http_client_set_max_redirects() | S10 | 2026-05-25 |
|| L07 | libhttp gzip/deflate decompression — auto-decompress via zlib | S10 | 2026-05-25 |
|| L12 | libcrypto AES-256-GCM encrypt/decrypt — crypto_aes_encrypt/decrypt | S10 | 2026-05-25 |
|| E05 | API server webhook endpoint — POST /webhook/:platform, JSON acknowledgment, 23 unit tests | S13 | 2026-05-25 |
|| L30 | libmcp streaming response — mcp_server_call_tool_stream, transport_read_any | S10 | 2026-05-25 |
|| L02 | libhttp connection pooling — pool_acquire/release, http_client_set_pool, LRU eviction | S10 | 2026-05-25 |
|| N02 | Mixture of Agents tool — 4 reference models + aggregator, OPENROUTER_API_KEY | S20 | 2026-05-25 |
|| D22 | Feishu doc/drive tool support — feishu_doc_read + feishu_drive_list tools | S7 | 2026-05-25 |
|| E02 | Token-buffer SSE streaming — ~4-char chunked streaming, UTF-8 safe | S13 | 2026-05-25 |
|| E01 | REST API config/service/metrics endpoints — handle_config_get, handle_service_info, handle_metrics_get, request counter, start time | S13 | 2026-05-25 |
||| L04 | libhttp multipart form data — builder API, boundary generation, http_post_multipart | S10 | 2026-05-25 |

## Next Priority Queue (top 10)

| Rank | ID | Description | LOC | Sector |
|------|----|-------------|-----|--------|
| 2 | U02 | TUI session browser with search | 200 | S14 |
| 1 | T04-T25 | Test coverage for remaining untested modules | — | S12 |
| 2 | N03 | Feishu doc and drive tools | 250 | S20 |
| 5 | U04 | TUI config editor | 150 | S14 |
| 6 | U03 | Skill browser/list | 100 | S14 |
| 7 | U05 | Plugin manager | 100 | S14 |
| 8 | U06 | Gateway status dashboard | 120 | S14 |
| 9 | U07 | Cron job viewer | 80 | S14 |
| 10 | U08 | Log viewer (tail -f in TUI) | 100 | S14 |
