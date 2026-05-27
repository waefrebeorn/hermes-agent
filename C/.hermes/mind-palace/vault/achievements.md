# Slermes C Translation — Vault of Achievements

All completed work archived here. Clears the active gap list for fresh battleship generation.
Last updated: 2026-05-25

## Phase 60: CI/Integration Stale Cleanup + Static Analysis — S22 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| I01 | GitHub Actions CI for C build — already exists in c-build.yml (build + test + ASan + coverage + Docker + perf) | S22 (stale) | .github/workflows/c-build.yml — full CI pipeline with build, test-suite, ASan, coverage, Docker build, perf gate |
| I02 | ASan in CI — already exists as asan job in c-build.yml | S22 (stale) | c-build.yml lines 87-120: ASan build + test under sanitizer |
| I03 | Code coverage reporting — gcov/lcov integration exists | S22 (stale) | c-build.yml lines 185-223: coverage job with lcov HTML report |
| I05 | Benchmark regression detection — already exists as perf job | S22 (stale) | c-build.yml lines 307-330: perf job runs make perf-gate |
| I06 | Release workflow — c-release.yml publishes tagged releases | S22 (stale) | .github/workflows/c-release.yml — GitHub Release on v* tags |
| I07 | Docker build for C image — C/Dockerfile exists, built in CI | S22 (stale) | C/Dockerfile (45 LOC multi-stage), c-build.yml lines 175-183: docker build test |
| A32 | tool_dispatch_helpers — fully ported in lib/libtooldispatch/ (304 LOC) | S4 (stale) | lib/libtooldispatch/tool_dispatch_helpers.c — stateless dispatch utilities matching Python |

## Phase 61: Static Analysis in CI — I04 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| I04 | Added cppcheck static analysis to c-build.yml CI pipeline — runs after test suite, --enable=all, --error-exitcode=1, suppresses missingIncludeSystem | S22 | .github/workflows/c-build.yml — new "Static analysis (cppcheck)" step. Suite 228/0/21 |

## Phase 62: Stale Claims Sweep + Vault Key Rotation (2026-05-25)

### Stale Claims Retired

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| M01 | anthropic_adapter — thinking block → reasoning extraction, tool_use block parsing, tool format conversion all fully implemented in provider_anthropic.c | S5 (stale) | provider_anthropic.c:187-201 (thinking config), 203-222 (tool format conversion), 449-516 (content block parser extracts thinking→reasoning, text→content, tool_use→tool_calls) |
| M09 | model_metadata — provider_metadata.c has 25+ model entries with context_window, MODEL_CAP flags (VISION, FUNCTION_CALLING, STREAMING, THINKING, STRUCTURED_OUTPUT, CONTEXT_CACHING), input/output pricing | S5 (stale) | provider_metadata.c:57-90 (full model table), 112 (model_context_window()), 139 (context_window in JSON output) |
| M12 | prompt_builder AGENTS.md/CLAUDE.md loading — system_prompt.c has full context file loading with truncation detection, cutoff limits | S5 (stale) | system_prompt.c:513-590 (AGENTS.md + CLAUDE.md loading), agent_loop.c:832 (context file loading in loop) |
| M13 | stream_diag — Ttfb tracking, first_token_time, token_count, tokens_per_second, total_stream_time all fully implemented in llm_client.c | S5 (stale) | llm_client.c:1035-1038 (stream_ctx_t diag fields), 1058-1065 (first token timing), 1304-1314 (finalize_stream_diag) |
| G11 | slack rich formatting — slack_send_blocks() Block Kit support exists with blocks JSON array | S8 (stale) | slack.c:71-95 (slack_send_blocks with blocks parameter) |
| G14 | discord slash command registration — discord_register_slash_command() exists with full REST API | S8 (stale) | discord.c:212-255 (discord_register_slash_command) |
| G15 | telegram inline query mode — telegram_answer_inline_query() exists, inline_query parsed in update handler | S8 (stale) | telegram.c:303-315 (answerInlineQuery), 659 (inline_query parsing) |
| G19 | bluebubbles iMessage attachment handling — bluebubbles_send_attachment() fully implemented | S8 (stale) | bluebubbles.c:173-217 (send_attachment with file path, guid, name) |

### Gap Closed

| ID | Description | LOC | Priority | Evidence |
|----|-------------|-----|----------|----------|
| V01 | Key rotation — vault_rotate_key() decrypts with old passphrase, re-encrypts with new. Two modes: in-memory (data already loaded) and file-based. Full rollback on failure. 13 new tests | 60 | P2 | vault.c:345-405 (vault_rotate_key), tests/test_vault.c:192-242 (13 rotation tests), vault (50 tests) |


## Phase 63: Display Parity Session — Phase 0b (2026-05-25)

Closed 9 real gaps, identified 2 stale claims. Phase 0b reduced from 11 to 1 gap.

### Gaps Closed (V01-V09)

| ID | Feature | LOC | Evidence |
|----|---------|-----|----------|
| V01 | Skin engine — TrueColor hex palette, 5 built-in skins, branding API | ~350 | src/display/skin.c (skin_get_string), src/display/display_core.c (display_{set,get}_skin, display_branding), src/display/ansi.c (display_color_hex), include/hermes_skin.h |
| V02 | KawaiiSpinner — 15 verb sets, wings animation, skin-branded | ~250 | src/cli/spinner.c (15 verb_sets, kawaii_spinner_{init,step,done}), skin wiring |
| V03 | Banner — skin-driven panel border, gradient, balance/CLI/gateway stats | ~200 | src/cli/cli.c (display_panel_hex banner), src/display/skin.c (banner_*) |
| V04 | Status bar — TrueColor bg/fg, context coloring, skin wiring | ~150 | src/cli/cli.c (status bar display), skin wiring |
| V05 | Tool feed — skin tool_emoji display | ~100 | src/display/ansi.c (display_prefix), tool feed wiring |
| V06 | Response box — skin border color | ~80 | src/cli/cli.c (response box display) |
| V07 | Help formatting — skin-driven panel, aligned columns, box-drawing | ~300 | src/cli/cmd_help.c (rewritten with panel + column alignment) |
| V08 | 256-color palette — display_color_256(), 256_BG/256_FG escape sequences | ~80 | src/display/ansi.c:58-68 (display_color_256), include/hermes_display.h (256_BG, 256_FG) |
| V09 | Prompt symbol — skin_get_string("branding.prompt_symbol") wired into display | ~50 | src/cli/cli.c (prompt display wiring) |

### Stale Claims Removed (V11-V12)

| ID | Claim | Reality | Evidence |
|----|-------|---------|----------|
| V11 | "faces" — separate feature needed | Already done by V02 KawaiiSpinner (12 distinct face animations in spinner.c) | spinner.c: faces_bounce, faces_dance, etc. |
| V12 | Tool emoji registry — per-tool emoji not implemented | Already exists in skin.c: tool_emojis table has emoji for each tool | skin.c: tool_emojis[] |
### Feature Added

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| A10 | Vault integration into tool_config_get (step 4). Multi-source credential resolution: override → per-tool env → generic env → vault → NULL. Remaining gap: config file YAML at tools.<tool>.<key> needs hermes_config_t access from tool layer | S4 | tool_config.c:94-97, test_tool_config.c:245-287 (3 vault tests). 28/28 passed |

### Stale Claims Retired This Session

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| G12 | whatsapp interactive buttons — whatsapp_send_interactive_buttons() exists with header/body/footer/buttons support | S8 (stale) | whatsapp.c:121-160 |
| P05 | API server dispatch — handle_post_chat already dispatches through g_agent->llm at line 294 | S2 (stale) | api_server.c:294-314 (full llm_chat_completion call) |


## Phase 59: Cloud Metadata Endpoint Detection — S01 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| S01 | Added always-blocked cloud metadata endpoint patterns to url_is_always_blocked(): metadata.goog (Google Cloud), 100.100.100.200 (Alibaba Cloud), fd00:ec2 (AWS IPv6 metadata), ::ffff: (IPv4-mapped IPv6), 100.64 (CGNAT pre-DNS check). Complemented existing DNS-based checks in is_private_ipv4 (CGNAT 100.64.0.0/10 at line 335) and is_private_ipv6 (ULA fc00::/7 covers fd00:ec2 at line 353) | S19 | url_safety.c:url_is_always_blocked() — 5 new string patterns. Suite 228/0/21. 0 errors, 0 warnings |

## Phase 58: Skill Slash-Command Injection — A28 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| A28 | skill_commands.c — port Python skill_commands.py to C: scan skills dir, parse frontmatter, build /slug→name/desc/path cache, resolve user input (underscore→hyphen normalization), build formatted invocation messages with activation note/skill dir/supporting files/user args, re-scan with diff. Wired into commands_dispatch fallthrough (cli.c, tui_fullscreen.c) so unknown /cmd checks ~/.hermes/skills/ | S4 | skill_commands.c — 7 public API functions (skill_cmd_scan, get, get_all, resolve, build_message, rescan); commands.c — new commands_try_skill() function; cli.c + tui_fullscreen.c — fallback wiring. Suite 228/0/21. 0 errors, 0 warnings |

## Phase 57: Mixture of Agents Tool — N02 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| N02 | Mixture of Agents tool — queries 4 reference models (claude-opus-4.6, gemini-2.5-pro, gpt-5.4-pro, deepseek-v3.2) via C LLM provider infra, then aggregates via claude-opus-4.6. Uses OPENROUTER_API_KEY. Sequential query_model calls, JSON-safe output escaping. | S20 | src/tools/mixture_of_agents.c — handle_mixture_of_agents, query_model, json_escape |

## Phase 56: Feishu Doc/Drive Tools — D22 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D22 | Feishu doc/drive tool support — `feishu_doc_read` (GET /docx/v1/documents/{id}/raw_content), `feishu_drive_list` (GET /drive/v1/files). Auth via FEISHU_APP_ID + FEISHU_APP_SECRET env vars, tenant token caching. | S7 | src/tools/feishu_tools.c — handle_feishu_doc_read, handle_feishu_drive_list, feishu_get_token |

## Phase 54: REST API Config/Service/Metrics — E01 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| E01 | REST API endpoints — GET /v1/config (safe config fields), GET /v1/service/info (version, uptime, build, endpoint list, request count), GET /v1/metrics (total_requests, uptime). Wire into dispatch_request. Request counter in dispatch. g_start_time in api_server_start. | S13 | api_server.c:handle_config_get(), handle_service_info(), handle_metrics_get(), dispatch_request() __atomic counter, api_server_start() g_start_time |

## Phase 51: Modal Terminal Backend — D07 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D07 | terminal.c — Modal execution backend: `run_command_modal()` wraps commands in a Python Modal app, runs via `modal run` CLI. Schema updated to mention `modal` backend option. | S7 | terminal.c |

## Phase 53: MoA Config Keys — C11 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C11 | agent_config_t: moa_enabled, moa_model, moa_strategy, moa_workers — YAML path agent.mixture_of_agents.*, defaults: disabled/round_robin/3 | S9 | hermes.h, config.c |

## Phase 52: Codex Runtime Config — C08 + C17 stale retirement (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| C08 | `agent.codex_runtime` config key (auto\|codex_app_server) — struct field, default "auto", YAML reader at agent.codex_runtime path | S9 | hermes.h, config.c |
| C17 | Stale claim retired — checkpoint config already has 8 struct fields, defaults, YAML reader, diff tracking, export, schema all present | S9 | hermes.h:846-854, config.c:490-496/1515-1526/2084 |

## Phase 50: Computer Use Vision Routing — D11 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D11 | computer_use.c — Vision routing: if backend's `capture("vision", ...)` returns NULL, fall back to `capture("som", ...)` transparently. Adds `vision_fallback` flag and explanatory `note` to capture response so agent knows mode was downgraded. | S7 | computer_use.c dispatch_capture |

## Phase 49: Computer Use Backend Registry — D10 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D10 | computer_use.c — Modular backend registry: `cu_register_backend()` registration system, `cu_list_backends()` JSON listing, `cu_clear_backends()` cleanup, `CU_BACKEND` env var override, auto-registration of 5 backends (noop/x11/wayland/macos/windows) | S7 | hermes_computer_use.h, computer_use.c |

## Phase 48: Camofox State Management — D15 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D15 | camofox_state — session persistence: save/load/delete browser session state (CDP URL, user_id, session_key) to `<home>/browser_auth/camofox/sessions/<task_id>.json`. Recursive dir creation. 3 new tests (32 total). | S7 | camofox_state.h, camofox_state.c, test_camofox_state.c |

## Phase 47: Browser Supervisor — D14 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D14 | browser.c — Browser supervisor: CDP health monitoring via Browser.getVersion, connection state/configured/version/command stats, registered as `browser_supervisor` tool | S7 | browser.c:1185-1291, registry_init_browser |

## Phase 46: Voice Mode Test Coverage — T24 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T24 | `voice_mode.c` — 20 test coverage tests: voice enable/disable state machine, device configuration, ASR command config, voice_record/voice_transcribe null safety, voice_speak null safety, voice_listen no-crash, mode idempotency | S12 | tests/test_voice_mode.c, test_runner.sh added |

## Phase 45: Library Feature — L08 (2026-05-24)

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




## Phase 68: Phase 3 Tool Features — File Permissions (2026-05-26)

| ID | Description | Evidence |
|----|-------------|----------|
| F18 | file_permissions handler — stat + chmod | src/tools/file.c:635-705 (handle_perms: stat, chmod, metadata response) |
| F18 | Schema: path (req), mode (opt, octal) | src/tools/file.c:78-87 (SCHEMA_PERMS) |
| F18 | Returns mode, size, uid, gid, type, is_dir/file/link | src/tools/file.c:690-704 |
| F18 | 3 tests: stat existing file (mode+type), missing file error | tests/test_file.c:343-361 |
| F18 | Suite: 227/0/24 (file_tool 40→43) | test_runner.sh |
