# Hermes C Translation — Vault of Achievements

All completed work archived here. Clears the active gap list for fresh battleship generation.
Last updated: 2026-05-31

## Phase 1: Foundation (Sessions 1-20)

### Build System & Toolchain
- `Makefile` — single-command build (Phase 5 complete: 29MB ELF binary, 0 warnings)
- `test_runner.sh` — shell-driven test harness (237 tests, 0 failures, 0 skips)
- Library system — 58 lib directories with `-I include` compilation model
- Git workflow — pre-commit hooks (whitespace, end-of-file, merge conflicts, build compile check)

### Core Infrastructure
- `main.c` — entry point with gateway/cron/TUI/CLI dispatch
- `config.c` — YAML config loading with 200+ settings across 20+ sections
- `secrets.c` — multi-source secret resolution (env, config, keyring)
- `hermes_error.c` — error type system
- `hermes_tokenizer.c` — token counting
- `skills_hub.c` — skill hub sync from remote

## Phase 2: Agent Core (Sessions 21-40)

### Agent Loop — `src/agent/agent_loop.c`
- Full synchronous conversation loop (init → tools → LLM → tools → ...)
- Message array management with append/free lifecycle
- Session persistence (auto-save on exit, manual /save, /load)
- Turn counting, tool call counting, iteration budget
- Compression integration (cooldown, tail messages, strategy)
- Title auto-generation
- Prompt caching fields

### Provider System — `src/agent/provider.c` + 9 provider modules
- Provider registry: 9 provider backends with unified interface
- OpenAI provider (chat completions + streaming + response_format)
- Anthropic provider (thinking blocks + streaming content deltas)
- Google provider (parts array + functionCall)
- DeepSeek provider (FIM builder + reasoning_effort)
- xAI provider (URL building + retirement header + reasoning_effort)
- Azure provider (API version config + custom domain)
- Bedrock provider (Converse API message conversion + SigV4 signing)
- OpenRouter provider (URL rewrite + reasoning_tokens)
- Custom provider (passthrough for arbitrary endpoints)

### All Provider Tests — `tests/test_provider_*.c`
- 43 tests: provider registry lifecycle
- 54 tests: OpenAI parse/stream/headers
- 74 tests: Anthropic thinking/streaming/headers
- 64 tests: Google parts/functionCall
- 60 tests: DeepSeek FIM/reasoning
- 63 tests: xAI URL/headers/parse/retirement
- 54 tests: Azure URL/headers/parse
- 40 tests: Bedrock Converse API + SigV4
- 50 tests: OpenRouter URL/headers/reasoning
- 37 tests: Custom passthrough
- 30 tests: Plugin extension lifecycle
- 73 tests: Tírith security scanning
- 30 tests: Session CRUD + db.c safety

### Additional Test Files
- 23 tests: Cronjob schedule validation
- 49 tests: Shell hooks JSON parsing + lifecycle
- 52 tests: Curator state management
- 72 tests: Hook registry callback dispatch
- 72 tests: Usage/pricing token cost estimation
- 9 tests: Subdirectory hint discovery

## Phase 3: CLI & Commands (Sessions 41-55)

### CLI Shell — `src/cli/cli.c`, `main.c`
- Tab completion (bash/zsh)
- Command history with navigation
- Table-style output rendering
- Raw mode terminal with signals (SIGINT/SIGTERM)
- Skin/theming system
- ANSI color support
- Multi-line input editing

### Command Handlers (79 commands in `commands.c`):
- `cmd_help` — per-command help
- `cmd_exit` / `cmd_clear` / `cmd_redraw` — terminal control
- `cmd_model` / `cmd_reasoning` / `cmd_fast` — LLM config
- `cmd_sessions` / `cmd_save` / `cmd_load` — session management
- `cmd_stats` / `cmd_usage` / `cmd_insights` — session analytics (cost per model, top sessions, daily activity, tool breakdown, source filter)
- `cmd_conv` / `cmd_new` / `cmd_undo` / `cmd_retry` / `cmd_branch` — conversation flow
- `cmd_config` / `cmd_profile` / `cmd_secrets` — configuration
- `cmd_history` / `cmd_logs` / `cmd_dump` — debugging
- `cmd_skills` / `cmd_plugins` / `cmd_tools` / `cmd_toolsets` — tool inspection
- `cmd_approve` / `cmd_deny` — approval flow
- `cmd_compress` / `cmd_snapshot` — context compression
- `cmd_cron` / `cmd_background` / `cmd_queue` — async tasks
- `cmd_goal` / `cmd_subgoal` / `cmd_bundles` — agent direction
- `cmd_restart` / `cmd_reload` / `cmd_copy` — lifecycle
- `cmd_indicator` / `cmd_statusbar` / `cmd_footer` / `cmd_busy` — UI toggles
- `cmd_voice` / `cmd_image` — voice/image commands
- `cmd_steer` / `cmd_kanban` / `cmd_platform` — specialization
- `cmd_send` / `cmd_session_search` / `cmd_session_export` — communication

### Bug Fix Batch 1
- B01: Buffer overflow in `--insights` path (`meta_to_json`, `sz` moved before `malloc`) — committed 68c8b9e9c
- S14: Approval allowlist substring-only → glob pattern matching — committed 410e17cc4
- S15: `~user` tilde expansion via `getpwnam()` — committed 3d3f525ce

## Phase 4: Tools (Sessions 56-70)

### Tool System — 85 registered tools
See `src/tools/` for individual implementations:
- `terminal.c` — Local/Docker/Sudo execution, timeout, cwd, pty, background, env passthrough
- `file.c` — Paginated reading, write with syntax linting, ripgrep-backed search
- `web.c` — HTTP GET with auth/cookies, DuckDuckGo/Tavily search, content extraction
- `patch.c` — Find-and-replace with 9 fuzzy strategies
- `exec_code.c` — Python execution with tool access
- `clarify.c` — User clarification dialog
- `memory.c` — Durable memory across sessions, plugin interface, file backend
- `todo.c` — JSON-backed task list
- `process.c` — Background process management
- `send_message.c` — Multi-platform message delivery
- `cronjob.c` — Cron scheduling
- `skill_mgmt.c` — Skill CRUD
- `session_search.c` — FTS5 session search
- `session_crud.c` — Session create/read/update/delete
- `tts.c` — Multi-provider TTS
- `vision.c` — Image analysis
- `delegate.c` — Subagent spawning
- `x_search.c` — X/Twitter search
- `browser.c` — 13 browser tools (navigate, snapshot, click, type, scroll, CDP with WebSocket/JSON-RPC)
- `computer_use.c` — Desktop automation (X11/xdotool, Wayland/ydotool/type, Noop)
- Set-value stubs replaced with type-based fallback on X11/Wayland — committed e7e116267
- `approval.c` — Approval cache queries, glob pattern allowlist
- `voice_mode.c` — Voice I/O, audio transcription
- `image_gen.c` — FAL.ai image generation
- `video_gen.c` — FAL.ai video generation
- `homeassistant.c` — Home Assistant integration
- `kanban.c` — 9 Kanban operations
- `discord.c` — Discord community interaction
- `file_batch.c` — Batch file operations
- `mcp_tool.c` — MCP server tool delegation
- `transcribe.c` — Audio file transcription
- `skills.c` — 12 skill operations (scan, validate, provenance, sync, bundle, usage, cache, search, curator, deps, list, hub)

### Tool Depth Gaps Closed
- L15-L17: Message-level DB queries (`db_query_tool_stats`) — committed 8b8499fec
- D01: Tool breakdown in `/insights` — committed 8b8499fec
- D03: `--source` filter for insights — committed 3aa0f8116

### Bug Fix Batch 2
- Skills.c: Use-after-free in `skills_list_handler` — committed 80d5746d0
- xai_provider.c: Added `reasoning_effort` from config — committed 91f1206de
- http_client.c: NO_PROXY env var bypass — committed 6fe669400
- patch.c: Fuzzy match at offset 0 buffer overflow guard bypass — committed 37368f987

## Phase 5: Gateway Platforms (Sessions 71-76)

### 19 Gateway Platforms
| Platform | LOC | Status |
|----------|-----|--------|
| telegram | 1129 | Full — send/edit/poll/keyboard/callback |
| discord | 655 | Full — message/slash/thread |
| webhook | 997 | Full — HMAC verify/send |
| slack | 389 | Send/poll |
| matrix | 485 | Send/poll |
| mattermost | 191 | Send/poll |
| whatsapp | 348 | Send/poll |
| email | 2083 | Full — IMAP/SMTP/SEND |
| signal | 373 | Send/poll |
| sms | 373 | Send/poll |
| homeassistant | 133 | Basic — input_text notify |
| feishu | 736 | Full — message/upload/callback |
| wecom | 338 | Send/poll |
| dingtalk | 237 | Send/poll |
| qqbot | 216 | Send, webhook-only |
| bluebubbles | 431 | Send/poll |
| msgraph_webhook | 446 | Full — webhook lifecycle |
| weixin | 537 | Full — send/poll |
| yuanbao | 559 | Poll/auth/message |

### R01: Curator depth
- `llm_background_review` wired to `/curator run` — committed 0f8ad0e1f

## Phase 6: Library Ports (Sessions 1-79)

### Dependency-Free Libraries (58 lib dirs)
All ported from Python dependencies:
- `libjson` — JSON parser/serializer
- `libhttp` — HTTP client with proxy auth, timeout, NO_PROXY, chunked
- `libyaml` — YAML parser
- `libcrypto` — SHA256/hex/base64
- `libdb` — JSON-file session store with metadata listing, message-level querying
- `libcsv` — CSV parser
- `libhash` — Hash table
- `libuuid` — UUID v4
- `libbase64` — Base64 encode/decode
- `libhtml` — HTML strip
- `libtextwrap` — Text wrapping
- `libglob` — Glob pattern matching
- `libregex` — POSIX regex wrapper
- `libansi` — ANSI strip/format
- `libsignal` — Signal handling
- `libdatetime` — Date/time formatting
- `libpath` — Path manipulation
- `libmcp` — MCP protocol client
- `libwebsocket` — WebSocket client
- `libplugin` — Dynamic plugin loader (.so)
- `libskin` — Skin/theming engine
- `libdotenv` — .env file reader
- `libcron` — Cron expression parser
- `libproc` — Process listing
- `libtui` — ncurses TUI framework
- `libdifflib` — Diff/match utilities
- `libjson5` — JSON5 parser
- `libbinary` — Binary file reading
- `libbrowser` — Camofox browser state
- `libdebug` — Debug helpers
- `libosv` — OSV vulnerability check
- `libwebsite` — Website policy
- `libtemplate` — Template rendering
- `libskillusage` — Skill usage tracking
- `libskillsync` — Skill sync
- `libtranscribe` — Audio transcription
- `libmcp_oauth` — MCP OAuth flow
- `libfal_common` — FAL.ai HTTP utilities
- `libtooloutput` — Tool output formatting
- `libxai_http` — xAI HTTP helpers
- `libenvpassthrough` — Env passthrough for subprocesses
- `libcredential` — Credential file management
- `libschemasanitizer` — Schema sanitization
- `libfuzzymatch` — Fuzzy string matching
- `libinterrupt` — Interrupt handling
- `libtoolbackend` — Tool backend dispatch
- `libmangateway` — Managed gateway
- `libratelimit` — Rate limiting (per-tool, per-provider)
- `librateguard` — Rate guard
- `libfilestate` — File state tracking
- `libtooldispatch` — Tool dispatch helpers
- `liberrorclassifier` — Error classification
- `libskillutils` — Skill utilities
- `liblineedit` — Line editing with history
- `libncurses` — Bundled ncurses for TUI
- `libenum` — Enum helpers
- `libprotobuf` — Protobuf decoder
- `libtoml` — TOML parser

## Phase 7: Plugin System

### 10 C Plugins
- `plugin_achievements.c` — Gamified achievement tracking
- `plugin_disk_cleanup.c` — Disk space management
- `plugin_file_memory.c` — File-backed memory provider
- `plugin_google_meet.c` — Google Meet integration
- `plugin_honcho.c` — Honcho memory backend
- `plugin_image_gen.c` — FAL.ai image gen (production curl, test mock)
- `plugin_kanban.c` — Kanban board manager
- `plugin_observability.c` — Metrics/traces/logs
- `plugin_skills.c` — Skill management
- `plugin_spotify.c` — Spotify playback control

## Phase 8: Stale Battleship Claims Retired

The following items from battleship-v7 were verified as stale — either already implemented before the audit, or code existed but was mislabeled:

| ID | Claim | Reality | Evidence |
|----|-------|---------|----------|
| D09 | "CUA backend missing" | Already implemented (computer_use.c:760-1148) | grep showed real macOS CUA functions |
| D12 | "CDP backend stub" | Real CDP with WebSocket/JSON-RPC (browser.c:1213) | Full handler with connect/send/parse |
| S01 | "stub_cdp_handler" | Real browser CDP code | browser.c:1168+ has full implementation |
| S02 | "cdp_send_command stub" | Real WebSocket JSON-RPC | browser.c:1213+ has connect/send |
| S03 | "x11_set_value not supported" | Type-based fallback implemented | committed e7e116267 |
| S04 | "wayland_set_value not supported" | ydotool + type fallback implemented | committed e7e116267 |
| S06 | "MCP sampling not supported" | Real implementation at mcp.c:1702 | Grep confirmed real handler |
| S17 | "noop backend fake" | Real testing backend (370+ LOC) | computer_use.c:33-150 full noop backend |
| W01 | "llm_background_review unwired" | Wired to /curator run | committed 0f8ad0e1f |
| R01 | "curator run fake" | Real llm_background_review wired | committed 0f8ad0e1f |
| B01 | "buffer overflow insights" | Fixed | committed 68c8b9e9c |
| D01 | "tool breakdown missing" | Added | committed 8b8499fec |
| D03 | "source filter missing" | Added | committed 3aa0f8116 |
| L15-L17 | "message-level DB queries missing" | Added | committed 8b8499fec |
| S14 | "approval substring-only" | Glob matching added | committed 410e17cc4 |
| S15 | "tilde expansion missing" | getpwnam() added | committed 3d3f525ce |

## Phase 9: Battleship-v8 Stale Claims Retired (2026-05-24)

The following items from battleship-v8 were verified as stale — code already implemented but audit missed the updates:

| ID | Claim | Reality | Evidence |
|----|-------|---------|----------|
| S05 | /curator run — fake run, "agent review not yet wired" | Real llm_background_review + curator_record_run | commands.c:2433-2477 full implementation |
| B05 | Gateway crash on malformed callback (telegram.c) | Null checks exist on callback dispatches | telegram.c has null-guard patterns |
| B06 | Memory leak in db_list_with_meta on error | Code clean — ids freed on all paths | db.c:515-527 proper cleanup |
| C01 | display.typing_indicator config missing | Covered by display.indicator (kaomoji/dots/minimal) | config.c:1071-1073 YAML reader |
| C02 | agent.skill_search_paths config missing | Has default + YAML reader + env override | config.c:1044-1048, 2053-2054 |
| C04 | compression.cooldown_secs no config reader | Has YAML reader + default | config.c:428 default, 1161-1162 YAML |
| C05 | compression.tail_messages no config reader | Has YAML reader + default | config.c:386 default, 1028-1029 YAML |
| C16 | Vision overrides config missing | Has YAML + env override | config.c:704-711, 975-979 |
| B03 | WSL path translation missing | Implemented — C:\→/mnt/c/ in file.c | committed 0f47bf8ea |
| B10 | Process health check missing | Implemented — action:health in process.c | committed d3580d886 |
| D04 | Insights empty-state handling missing | Implemented — graceful no-sessions message | committed in session |
| R04 | HomeAssistant poll reset missing | Implemented — input_text reset after poll | committed in session |
| P01 | Anthropic ephemeral cache headers | Implemented — anthropic-beta header | committed in session |
| D17-D20 | File backend import/export/hash/compress/prioritize | All have real implementations | memory.c:1164-1350 |

## Phase 10: CLI Commands Depth (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D02 | Skill breakdown in insights — shows loaded skills with usage stats (use/view/patch counts) | S7 | commands.c:2830-2857 — skill_usage_load + active/total counts |

## Phase 11: Agent Module Ports
### Stale Claims Retired

| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| A27 | rate_limit_tracker — battleship claimed unported | Already ported in lib/libratelimit/rate_limit.c | rate_limit.h:112 — full API with header parsing + formatting + tests | (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| A23 | nous_rate_guard — cross-session Nous Portal rate limit guard with atomic JSON state file | S4 | nous_rate_guard.h/c, test_nous_rate_guard.c — 11/11 tests |

## Phase 18: WeCom Inbound Polling — G05 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| G05 | wecom_poll_messages — implemented webhook message queue + handle_webhook + poll function; wired thread_poll_wecom in server.c to poll messages; uses existing get_app_token() infrastructure | S8 | wecom.c:328-420, server.c:1408-1425 |



| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| G04 | dingtalk_poll_messages — implemented OAuth2 access token acquisition + webhook message queue + poll function; wired thread_poll_dingtalk in server.c to poll messages | S8 | dingtalk.c:224-380, server.c:1427-1445 |

## Phase 18: WeCom Inbound Polling — G05 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| G05 | wecom_poll_messages — implemented webhook message queue + handle_webhook + poll function; wired thread_poll_wecom in server.c to poll messages; uses existing get_app_token() infrastructure | S8 | wecom.c:328-420, server.c:1408-1425 |

## Phase 19: SMS Webhook Wiring — G06 (2026-05-24)
## Phase 20: Mattermost Bot-Message Filtering — G07 (2026-05-24)
## Phase 21: QQ Bot Inbound Polling — G02 (2026-05-24)



| ID | Description | Sector | Evidence |

|----|-------------|--------|----------|

| G02 | qqbot_poll_messages — implemented webhook message queue + handle_webhook (OneBot + QQ Guild API formats); wired thread_poll_qqbot in server.c | S8 | qqbot.c:206-310, server.c:1492-1510 |



| ID | Description | Sector | Evidence |

|----|-------------|--------|----------|

| G07 | mattermost_poll_messages — added bot user ID lookup via /users/me, self-message filtering; removed dedup comment | S8 | mattermost.c:23,135-160 |

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| G06 | sms_poll_messages — implemented webhook message queue + handle_webhook using existing sms_parse_webhook; wired thread_poll_sms in server.c to poll and process; bridge between Twilio webhook POST and gateway update pipeline | S8 | sms.c:348-418, server.c:1372-1390 |

## Current Verified State

| Metric | Value | Verified |
|--------|-------|----------|
| Suite | 237/0/0 | ✅ runtime verify |
| Binary | 29MB ELF, 0 errors, 0 warnings | ✅ build verify |
| Source .c files | 153 | ✅ ls verify |
| Headers | 66 | ✅ ls verify |
| Library dirs | 58 | ✅ ls verify |
| Test files | 202 | ✅ find verify |
| Tools registered | 85 | ✅ registry grep |
| CLI commands | 79 | ✅ grep verify |
| Gateway platforms | 19 | ✅ ls verify |
| Agent .c modules | 50 | ✅ ls verify |
|| C provider modules | 11 | ✅ ls verify |
|| Provider test files | 11 | ✅ ls verify |
|| Plugins | 10 | ✅ ls verify |
|| Git commits | 847+ | ✅ log verify |
|| Python agent modules | 77 | ✅ ls verify |
|| Python tools | 88+ | ✅ ls verify |

## Phase 22: Battleship-v8 Stale Claims Retired — Round 2 (2026-05-24)

ID | Old Claim | Reality | Evidence
----|-----------|---------|----------
G01 | homeassistant — "No conversation loop, one-way notify only" | `ha_poll_messages()` exists with full HTTP polling loop | homeassistant.c:74
D16 | memory.c — "Plugin memory provider interface" | `memory_storage_plugin_init()` with plugin registry, full prototype in header | hermes_memory.h:318-322
G10 | signal — "Attachment support" | `signal_send_attachment()` + attachment parser in inbound handler | signal.c:162, 275-282, 370-372
L03 | libhttp — "Retry with backoff" | `http_new_with_retry()`, `max_retries`, `backoff_ms`, exponential backoff sleep | libhttp/http.c:47-48, 257-262, 392-393

## Phase 23: Config Key Implementation — C06 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
C06 | gateway.secret_rotation — added `int secret_rotation_interval` to hermes_config_t, YAML reader via `yaml_get_int(doc, "gateway.secret_rotation", 0)`, default 0 (disabled) | S9 | hermes.h:959, config.c:1092

## Phase 24: Config Key Implementation — C07 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
C07 | tools.environments — added `char environments[512]` to tools_config_t, YAML reader via `yaml_get_string(doc, "tools.environments")` | S9 | hermes.h:459, config.c:1015

## Phase 26: Config Key Implementation — C10 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C10 | plugins.memory.provider — added `char memory_provider[128]` to plugin_config_t, YAML reader via `yaml_get_string(doc, "plugins.memory.provider")` | S9 | hermes.h:574, config.c:1230

## Phase 28: Config Key Implementation — C12 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C12 | credentials.sources — added `char credential_sources[512]` to hermes_config_t, YAML reader via `yaml_get_string(doc, "credentials.sources")` | S9 | hermes.h:961, config.c:1095

## Phase 29: Config Key Implementation — C13 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C13 | gateway.signal.number — added `char signal_number[64]` to hermes_config_t, YAML reader via `yaml_get_string(doc, "gateway.signal.number")` | S9 | hermes.h:962, config.c:1097

## Phase 30: Config Key Implementation — C14+C15 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C14 | proxy.https_proxy — added `char proxy_https[512]` to hermes_config_t, YAML reader via `yaml_get_string(doc, "proxy.https_proxy")` | S9 | hermes.h:963, config.c:1099 |
| C15 | proxy.no_proxy — added `char proxy_no[1024]` to hermes_config_t, YAML reader via `yaml_get_string(doc, "proxy.no_proxy")` | S9 | hermes.h:964, config.c:1102 |

## Phase 31: Config Key Implementation — C18 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C18 | agent.vault.path — added `char vault_path[512]` to hermes_config_t, YAML reader via `yaml_get_string(doc, "agent.vault.path")` | S9 | hermes.h:965, config.c:1103 |

## Phase 27: Stale Claim Retired — B08 (2026-05-24)

| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| B08 | MCP tool auth tokens not persisted | `credential_store_save()` exists — loads `mcp_auth.json`, updates token entry, saves to temp file with atomic rename | mcp_tool.c:1052-1106 |

## Phase 25: Stale Claim Retired — C09 (2026-05-24)

| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| C09 | agent.bitwarden — "Bitwarden secrets manager config" | Already implemented as `secrets.bitwarden.*` with 5 config fields, YAML reader, defaults, diff tracking, and validation | hermes.h:857-861, config.c:1516-1523, config.c:2309 |

## Phase 12: A02 Context Compressor Closure (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| A02 | context_compressor — core compression pipeline (tool pruning, redaction, LLM summary gen, scaled budget, boundary alignment, anti-thrashing, summary prefix) | S4 | llm_client.c:385-761, agent_loop.c:880-930, boundary alignment added in session |
| A03 | conversation_compression — compression orchestration (agent_loop.c compression loop, auxiliary_client.c model probe, anti-thrashing, summary wrapping, message truncation) | S4 | agent_loop.c:880-930, auxiliary_client.c, context_engine.c, llm_client.c |
| S07, S08 | Plugin memory vtable import_json + export_json — wired in plugin_vtable | S1 | memory.c:1472-1473 (NULL→plugin_import_json/plugin_export_json) |
| S12 | plugin_delete — now calls plugin interface instead of returning false | S1 | memory.c:1412-1414 (return false→memory_store with delete metadata) |

## Phase 13: Plugin Vtable Stub Closure (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| S09 | plugin_get_by_hash — replaced NULL (potential segfault) with safe fallback returning false (best-effort dedup for plugin backend) | S1 | memory.c:1510-1523 (new function plugin_get_by_hash, wired in plugin_vtable) |
| S10 | plugin_compress_old — replaced NULL with no-op returning 0 (plugin manages own state) | S1 | memory.c:1525-1530 (new function plugin_compress_old) |
| S11 | plugin_get_prioritized — replaced NULL with safe fallback returning NULL/0 (plugin doesn't support priority enumeration) | S1 | memory.c:1532-1538 (new function plugin_get_prioritized) |

## Phase 14: Stale Stub Retirement — S13, S16 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| S13 | plat.shutdown = NULL — stalely listed as stub; gw_platform_shutdown_all() has NULL check, polling platforms don't need cleanup | S1 | server.c:941 (`if (shutdown)` guard check exists), comment at line 1729 explains intentional NULL |
| S16 | fal_provider.generate = NULL — stalely listed as stub; tool uses video_generate_handler directly, no `->generate()` call exists | S1 | video_gen.c:219-220 (NULL with comment), no callers of `->generate` in codebase |
| W13 | plat.shutdown = NULL — duplicate of S13, removed from dead-code sector | S3 | Removed from battleship SECTOR 3 as duplicate stale claim |

## Phase 15: Skill Bundle Apply — W09 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| W09 | skill_bundle_apply() — implement apply/install for skill bundles (was dead code: parsed but couldn't install) | S3 | skill_bundles.c:200-216, skill_bundles.h:57-60 |

## Phase 16: Title Generation Improvement — P07 (2026-05-24)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| P07 | title.c — improved extractive title: first sentence with code-block skipping, whitespace collapsing, 80-char cap, sentence-ending punctuation detection (was placeholder: "(void)cfg — Title gen could use LLM in future") | S2 | title.c:12-76, test_title.c: tests updated for sentence-based extraction |
