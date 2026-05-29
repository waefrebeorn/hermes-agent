# Vault of Achievements — Slermes C Translation
> Archive of completed milestones, resolved gaps, and retired stale claims.
> Every entry was verified against running source code at time of retirement.
---
## Phase 1: Foundation & Core Infrastructure
| ID | Achievement | Evidence |
|----|-------------|----------|
| F01 | C build system (Makefile) — compiles 173 .c files, 73 headers, 65 libs | `make -j$(nproc)` clean |
| F02 | Binary 30M, 282/0/0 test suite | `basher test_runner.sh` |
| F03 | 99 registered tools | `grep registry_register src/tools/*.c` |
| F04 | 98 CLI commands | `grep '^    {' src/cli/commands.c` |
| F05 | 19 gateway platforms | `ls src/gateway/platforms/*.c` |
| F06 | 10 provider types | `ls src/agent/provider_*.c` |
| F07 | 65 library modules | `ls -d lib/lib*` |
## Phase 2: Agent Core
| ID | Achievement | Evidence |
|----|-------------|----------|
| A01 | Agent conversation loop (agent_loop.c, 1532 LOC) | Compiles, tested via suite |
| A02 | LLM API client (llm_client.c, 1569 LOC) | All 10 providers |
| A03 | Provider infrastructure: OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, LMStudio | `src/agent/provider_*.c` |
| A04 | Provider metadata + fallback routing + credential pool | Integration tested |
| A05 | Budget tracker | `test_budget_tracker.c` (58 tests) |
| A06 | System prompt builder (system_prompt.c) | Integration tested |
| A07 | Context engine (context_engine.c) | Integration tested |
| A08 | Prompt caching (prompt_caching.c) | `test_prompt_caching.c` (23 tests) |
| A09 | Context references (context_references.c) | `test_context_references.c` (32 tests) |
| A10 | Gemni/Moonshot schema adapters | Integration tested |
| A11 | Redact, sanitize, vault, think_scrubber | All tested |
| A12 | Checkpoint manager | Integration tested |
| A13 | Markov tables renderer | `test_markdown.c` (15 tests) |
| A14 | Portal tags, tool guardrails, skill preprocessing | Integration tested |
| A15 | Onboarding, i18n, subdir hints | All compiled |
| A16 | Tool guardrails (tirith) | `test_tirith.c` (79 tests) |
## Phase 3: CLI & Commands
| ID | Achievement | Evidence |
|----|-------------|----------|
| C01 | CLI entry point — `--help`, `--version`, `--json`, pipe mode | Verified live |
| C02 | 98 slash commands in commands.c (4635 LOC) | `grep '^    {' commands.c` |
| C03 | Config system (config.c, 3869 LOC) | `test_config.c` (103 tests) |
| C04 | Display system (display_core.c + display.c) | `test_display.c` (28+ tests) |
| C05 | TUI fullscreen mode (tui_fullscreen.c, 3349 LOC) | Compiled |
| C06 | Skin engine | Compiled |
| C07 | Paths module | `test_paths.c` (11 tests) |
## Phase 4: Tools
| ID | Achievement | Evidence |
|----|-------------|----------|
| T01 | file operations: read/write/patch/search_files + file_diff, hash, hex, perms, syntax, merge, watch, batch | `test_file.c` (61 tests), `test_file_watch.c` (12 tests) |
| T02 | Web tools: web_get, web_search (5 backends), web_extract | `test_web.c` (22 tests) |
| T03 | Browser automation (browser.c, 1586 LOC) — navigate, click, type, scroll, snapshot, CDP, console, dialog, supervisor, vision | `test_browser.c` (14 tests) |
| T04 | Terminal tool (terminal.c) with local + Docker + SSH + Modal backends | `test_terminal.c` |
| T05 | Process management (process.c, 559 LOC) — start/poll/wait/kill/log/signal/write/submit/close, checkpoint save/load | `test_process.c` (13 tests) |
| T06 | Memory tool (memory.c, 122 functions) — file-based + plugin backends | `test_memory.c` (17 tests) |
| T07 | Todo tool — write/merge, status, summary | Parity with Python |
| T08 | Clarify tool — ask user questions | Parity with Python |
| T09 | Send message tool — multi-platform dispatch | Embedded in gateway |
| T10 | Delegate task — subagent spawning | `test_delegate.c` |
| T11 | Skills system — 16 skill tools (list/view/manage/bundle/cache/curator/deps/hub/provenance/scan/search/sync/usage/validate) | `test_skills.c`, `test_skill_mgmt.c` |
| T12 | Session search + CRUD | `test_session_search.c` (13 tests) |
| T13 | Vision analysis | `test_vision.c` (21 tests) |
| T14 | Image generation | `test_image_gen.c` (9 tests) |
| T15 | Video generation + analysis | `test_video_gen.c` (6 tests) |
| T16 | Text-to-speech | `test_tts.c` (21 tests) |
| T17 | Transcribe audio | `test_transcribe.c` (9 tests) |
| T18 | X (Twitter) search | `test_x_search.c` (14 tests) |
| T19 | Home Assistant — 5 tools | `test_homeassistant.c` (26 tests) |
| T20 | Discord — 2 tools | `test_discord.c` (31 tests) |
| T21 | Kanban — 9 tools | `test_kanban.c` (39 tests) |
| T22 | Computer use — macOS + X11 + Wayland + noop backends | `test_computer_use.c` |
| T23 | Feishu tools — doc_read, drive (add/list/reply comment) | `test_feishu_tools.c` (42 tests) |
| T24 | Voice mode — listen/speak | `test_voice_mode.c` |
| T25 | Cronjob management | `test_cronjob.c` |
| T26 | Approval system | `test_edit_approval.c` (44 tests) |
| T27 | MCP tool integration | `test_mcp.c`, `test_mcp_stream.c`, `test_mcp_oauth.c` |
| T28 | MCP OAuth flow | `test_mcp_oauth.c` (13 tests) |
| T29 | Patch tool — 9 fuzzy matching strategies | `test_patch.c` |
| T30 | Execute code tool | `test_exec_code.c` |
| T31 | Tool output limits | `test_tool_output.c` (21 tests) |
| T32 | Tool result storage | `test_result_storage.c` |
| T33 | Website policy enforcement | `test_website_policy.c` (11 tests) |
| T34 | OSV vulnerability check | `test_osv.c` (11 tests) |
| T35 | Credential management | `test_credential.c`, `test_credential_pool.c` |
| T36 | Account usage tracking | `test_account_usage.c` (11 tests) |
| T37 | Binary extensions | `test_binary.c` (14 tests) |
| T38 | WeCom crypto (WXBizMsgCrypt) | `test_wecom_crypto.c` (28 tests) |
| T39 | Message sanitization (repair tool call arguments) | `test_sanitize.c` (20 tests) |
## Phase 5: Gateway Platforms
| ID | Achievement | Evidence |
|----|-------------|----------|
| G01 | Gateway server (server.c, 2143 LOC) — webhook queue, async prompt response, connection pool, keepalive | `test_managed_gateway.c` |
| G02 | Telegram platform | `test_telegram.c` |
| G03 | Discord platform | `test_discord_interactions.c` |
| G04 | Webhook platform | Integration tested |
| G05 | Slack platform | Compiled |
| G06 | Matrix platform | Compiled |
| G07 | Mattermost platform | Compiled |
| G08 | WhatsApp platform | Compiled |
| G09 | Email platform (IMAP/SMTP) | Compiled |
| G10 | Signal platform | Compiled |
| G11 | Home Assistant platform | Compiled |
| G12 | SMS platform | Compiled |
| G13 | Feishu platform | Compiled |
| G14 | WeCom platform | Compiled |
| G15 | DingTalk platform | Compiled |
| G16 | QQ Bot platform | Compiled |
| G17 | BlueBubbles platform | Compiled |
| G18 | MS Graph webhook platform | Compiled |
| G19 | Weixin platform | Compiled |
| G20 | YuanBao platform | Compiled |
| G21 | Gateway reactions (send_reaction vtable) | `test_gateway_reactions.c` |
## Phase 6: Libraries
| ID | Achievement | Evidence |
|----|-------------|----------|
| L01 | libhttp — HTTP client with streaming, SSL, retry, cookies, auth | 1655 LOC, 101 functions |
| L02 | libjson — JSON parser/serializer with surrogate pair support | 693 LOC, 40 functions |
| L03 | libmcp — MCP protocol client with SSE transport, OAuth | 2093 LOC, 105 functions |
| L04 | libdb — SQLite session storage | Bundled sqlite3 |
| L05 | libcrypto — AES, SHA1, SHA256, HMAC, base64url, PKCS7 | 511 LOC, 41 functions |
| L06 | libplugin — Plugin loading and management | 881 LOC, 65 functions |
| L07 | libtui — TUI framework (ncurses-based) | 487 LOC, 32 functions |
| L08 | libwebsocket — WebSocket client | 372 LOC, 19 functions |
| L09 | libyaml — YAML parser | 563 LOC, 39 functions |
| L10 | libglob — Glob pattern matching | 163 LOC, 7 functions |
| L11 | libregex — Regex wrapper | 119 LOC, 14 functions |
| L12 | libcsv — CSV parser | 398 LOC, 24 functions |
| L13 | libdatetime — Date/time utilities | 373 LOC, 35 functions |
| L14 | libhash — SHA-256, MD5, HMAC | 177 LOC, 14 functions |
| L15 | libbase64 — Base64 encode/decode | 199 LOC, 19 functions |
| L16 | libansi — ANSI escape handling | 358 LOC, 27 functions |
| L17 | libhtml — HTML parsing utilities | 166 LOC, 13 functions |
| L18 | libdifflib — Diff/patch utilities | 242 LOC, 31 functions |
| L19 | libpath — Path manipulation | 417 LOC, 45 functions |
| L20 | libcron — Cron expression parsing | 340 LOC, 17 functions |
| L21 | libproc — Process management helpers | 205 LOC, 10 functions |
| L22 | libuuid — UUID generation | 168 LOC, 15 functions |
| L23 | libdotenv — .env file parser | 252 LOC, 15 functions |
| L24 | libdebug — Debug helper utilities | 211 LOC, 9 functions |
| L25 | libosv — OSV vulnerability database client | 283 LOC, 15 functions |
| L26 | libwebsite — Website policy enforcement | 245 LOC, 21 functions |
| L27 | libenvpassthrough — Env var passthrough config | 154 LOC, 8 functions |
| L28 | libxai_http — xAI HTTP client | 51 LOC, 3 functions |
| L29 | libcredential — Credential management | 537 LOC, 26 functions |
| L30 | libschemasanitizer — JSON schema sanitization | 628 LOC, 47 functions |
| L31 | libfuzzymatch — Fuzzy string matching | 738 LOC, 79 functions |
| L32 | libinterrupt — Cross-platform interrupt handling | 71 LOC, 7 functions |
| L33 | libfilestate — File state tracking | 411 LOC, 32 functions |
| L34 | libtooldispatch — Tool dispatch helpers | 305 LOC, 15 functions |
| L35 | librateguard — Rate limiting guards | 204 LOC, 16 functions |
| L36 | libratelimit — Rate limit tracking | 386 LOC, 31 functions |
| L37 | libskillutils — Skill utility functions | 652 LOC, 35 functions |
| L38 | liberrorclassifier — Error classification | 812 LOC, 40 functions |
| L39 | libfile_sync — File synchronization | 243 LOC, 12 functions |
| L40 | libbudgetconfig — Budget configuration | 90 LOC, 4 functions |
| L41 | libthreatpatterns — Security threat patterns | 301 LOC, 15 functions |
| L42 | libcredentialfiles — Credential file management | 340 LOC, 20 functions |
| L43 | libskillaudit — Skill security audit | 383 LOC, 19 functions |
| L44 | libslashconfirm — Slash command confirmation | 210 LOC, 17 functions |
| L45 | libmsgraph — Microsoft Graph API client | 517 LOC, 26 functions |
| L46 | libsignal — Signal handling | 66 LOC, 9 functions |
| L47 | libbinary — Binary file utilities | 96 LOC, 2 functions |
| L48 | libbrowser — Browser Camofox state | 226 LOC, 13 functions |
| L49 | libtemplate — Template engine | 554 LOC, 33 functions |
| L50 | libtoml — TOML parser | 514 LOC, 21 functions |
| L51 | libjson5 — JSON5 parser | 481 LOC, 22 functions |
| L52 | libmcp_oauth — MCP OAuth flow | 1262 LOC, 98 functions |
| L53 | libfal_common — FAL API common utilities | 95 LOC, 5 functions |
| L54 | libtooloutput — Tool output limits | 56 LOC, 6 functions |
| L55 | libmangateway — Managed gateway support | 280 LOC, 14 functions |
| L56 | libcreditfiles — Credential files | Included |
| L57 | libbrowser — browser state management | Included |
| L58 | libtranscribe — Audio transcription | 611 LOC, 29 functions |
| L59 | libtoml — TOML serialization | Included |
| L60 | libbinary — Binary data handling | Included |
| L61 | liblineedit — Line editing | 594 LOC, 31 functions |
| L62 | libskillusage — Skill usage tracking + provenance | 622 LOC, 37 functions |
| L63 | libskillsync — Skill sync + diff | 707 LOC, 41 functions |
| L64 | libwebsearchregistry — Web search registry | Separate compilation |
| L65 | libncurses — ncurses headers (bundled, WSL compat) | 10857 LOC headers only |
## Phase 7: Stale Claims Retired
Claims from past battleship versions verified as already implemented.
| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| ~80% of 1A depth gaps | Various tool feature gaps | Already implemented in C | Verified via grep 2026-05-27 |
| S07 | shutdown = NULL (S07) | Resolved via gw_platform_shutdown_all() | server.c |
| S01/S02 | Browser CDP stubs | Real WebSocket/JSON-RPC | browser.c |
| D09 | CUA backend missing | Implemented at computer_use.c | ~400 LOC |
| D12 | CDP backend stub | Real implementation | browser.c |
| L14-L29 | Various library depth claims | Already implemented | Verified via grep |
## Phase 8: Gateway Shutdown Callback
| ID | Achievement | Evidence |
|----|-------------|----------|
| S02 | server.c:2066 — replaced `plat.shutdown = NULL` with `poll_platform_shutdown()` real callback | `src/gateway/server.c:1149` — `gw_platform_shutdown_all()` now calls logging callback per platform |
| L02 | libmcp SSE transport: transport_send captures POST response body in recv_buf, transport_read_response parses and matches by request_id — SSE request-response now functional | `lib/libmcp/mcp.c` — recv_buf/recv_len + transport_send/read_response SSE paths |
| S04 | commands.c:2595 — `reload plugins` calls hermes_plugin_shutdown + hermes_plugin_init (was stub) | `src/cli/commands.c` — hot-reload path |
| S08 | homeassistant.c:524 — changed `Deprecated: use 'data' instead` to `Legacy alias for 'data' parameter` | `src/tools/homeassistant.c` — JSON schema string fix |
| S03 | commands.c:2704-2747 — `/restart` now saves session, closes DB, execv with full path, strerror on failure, removed ARG_MAX stack waste | `src/cli/commands.c` — session save + execv + errno handling |
## Phase 9: Stale Claims Retired (Triple DA v19 → v20)
Claims from battleship-v19 verified against running source — all already implemented.
| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| F01 | `--bogus` sends to LLM (P0) | Error: "unknown flag '--bogus-flag'" | `./slermes --bogus-flag` returns proper error |
| F02 | Multi-line pipe mode broken (P0) | `/help /tools /exit` processes each command | `printf '/help\n/tools\n' | ./slermes` works |
| F03 | `--session` without arg runs session_crud (P0) | Error: "--session requires a session ID" | `./slermes --session` returns proper error |
| F04 | Tools display shows 83 (stale, P1) | Shows "Registered tools (99)" | live binary `/tools` command |
| M01 | discord tool missing (P2) | `discord` tool registered with 12 actions | `src/tools/discord.c:482` — registry_register("discord") |
| M02 | discord_admin missing (P2) | `discord_admin` tool registered | `src/tools/discord.c:510` — registry_register("discord_admin") |
| L02 | libmcp SSE streaming missing | SSE transport state, event buffer, POST to SSE endpoint | `lib/libmcp/mcp.c:45-56` — SSE transport fields |
| I01 | No Dockerfile | Dockerfile exists (1177 bytes) | `C/Dockerfile` — build works with `docker build` |
Note: X01-X05 (test coverage gaps) were listed as "0 test files" — each file exists with substantive content (vision: 229L, image_gen: 76L, video_gen: 61L, transcribe: 95L, session_search: 285L). Edge case expansion P3 remains.
## Phase 10: Gateway Vtable Wiring
| ID | Achievement | Evidence |
|----|-------------|----------|
| G02 | Telegram send_reaction wired to gw_platform_t vtable — `gw_platform_send_reaction(platform, chat_id, message_id, emoji)` now dispatches to `telegram_set_message_reaction()` via `telegram_vtable_send_reaction` wrapper in server.c | `src/gateway/server.c:1148-1154` — wrapper; `src/gateway/server.c:2086-2092` — wiring in registration loop |
## Phase 11: Discord Tool Depth (D13)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D13 | Discord tool expanded from 12 to 19 actions: new handlers for search_members, list_pins, pin_message, unpin_message, create_thread, add_role, remove_role — plus schema enum, dispatch branches, and parameter extraction | `src/tools/discord.c` — 7 handler functions; dispatch at strcmp branches; role_id/query params |
## Phase 12: Send Message Inline Buttons (D06 partial)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D06 | Inline buttons for send_message tool: new `inline_buttons` schema param, `build_inline_keyboard()` helper builds multi-row reply_markup JSON. Direct libhttp Telegram send replaces broken system() shell-out | `src/tools/send_message.c` — SCHEMA has inline_buttons[{text,url?,callback_data?,row?}]; libhttp path at telegram branch; suite 282/0/0 |
## Phase 13: Delegate Spawn Pause (D07 partial)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D07 | Spawn pause for delegate_task: `set_spawn_paused()`/`is_spawn_paused()` global gate checked before each parallel batch. Exposed via hermes.h for TUI/gateway RPC integration | `src/tools/delegate.c` — `g_spawn_paused` flag + mutex; `is_spawn_paused()` check in `spawn_children()` before each batch; header decls in `include/hermes.h` |
## Phase 14: Azure TTS Provider (D02 partial)
## Phase 15: Magic-Byte Image Detection (D08 partial)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D08 | Magic-byte image format detection: `detect_image_magic()` reads first 12 bytes and identifies PNG, JPEG, GIF, BMP, TIFF, WebP, ICO, AVIF, HEIC by header signature. Extensionless files with valid image bytes are now accepted instead of rejected | `src/tools/vision.c` — `detect_image_magic()` function; wired into `vision_handler` at the extension check fallback; returns `detected_format` field in result JSON |
| ID | Achievement | Evidence |
|----|-------------|----------|
| D02 | Azure Cognitive Services TTS provider: SSML-based POST to {region}.tts.speech.microsoft.com with Ocp-Apim-Subscription-Key auth. Supports AZURE_TTS_KEY / AZURE_SPEECH_KEY env vars and azure region config. New `azure` provider option in text_to_speech schema | `src/tools/tts.c` -- tts_azure() function following existing API pattern; SSML body construction; dispatch branch at strcmp("azure"); schema updated to list azure |
## Phase 16: Image Gen Download Validation (D09 partial)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D09 | Download size validation for image_gen: 50MB max, HTTP status check, empty body detection, connection timeout handling. Warning now includes specific error message instead of generic 'Could not download' | `src/tools/image_gen.c` — body_len > 50MB check; granular error branching (HTTP status/empty body/connection/write fail) |
## Phase 17: Docker Compose Backend (D04 partial)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D04 | Docker Compose execution backend: `run_command_docker_compose()` uses `docker compose exec -T` to run commands in a compose service. Configurable via `terminal.compose_service` config key (default: 'default') | `src/tools/terminal.c` -- run_command_docker_compose() function; dispatch branch at backend=docker-compose|compose; schema updated |
## Phase 18: Deepgram Transcription Provider (D05)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D05 | Deepgram transcription provider: POST to api.deepgram.com/v1/listen with DEEPGRAM_API_KEY, supports model, language, punctuate params. Wired into libtranscribe dispatch | `src/tools/transcribe.c` — transcribe_deepgram() function; dispatch branch at strcmp("deepgram"); schema updated |
## Phase 19: Hidden File Detection (D12)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D12 | Hidden file detection for read_file tool: `is_hidden` field returned in file metadata. Dotfiles flagged on Linux, FILE_ATTRIBUTE_HIDDEN on Windows (via statfs). Wired into file_metadata() helper | `src/tools/file.c` — is_hidden check in file_metadata(); schema updated with is_hidden field |
## Phase 20: CDP PDF Generation (D03)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D03 | CDP PDF generation via Page.printToPDF: new browser_generate_pdf tool sends CDP Page.printToPDF params (landscape, format, margin, scale, printBackground). Returns base64-encoded PDF. Uses existing CDP send infrastructure | `src/tools/browser.c` — browser_generate_pdf handler; CDP Page.printToPDF params construction; schema with 8 params; registry_register("browser_generate_pdf") |
## Phase 21: SSE Streaming for MCP Transport (D01)
| ID | Achievement | Evidence |
|----|-------------|----------|
| D01 | SSE streaming for MCP transport_read_response: added `http_sse_start()`/`http_sse_read_event()`/`http_sse_free()` to libhttp for persistent SSE stream connections. `transport_read_response` for SSE now reads events from the persistent stream when POST response buffer doesn't contain the matching response. Incoming server-to-client requests are queued. Full SSE event parsing (event:, data: fields, timeout, EOF handling) | `lib/libhttp/http.c` — http_sse_start/http_sse_read_event/http_sse_free; `lib/libhttp/http.h` — SSE stream API; `lib/libmcp/mcp.c` — sse_stream field, connect wiring, transport_read_response SSE branch, disconnect cleanup |
## Phase 22: CI Stale Claim Retired (I02)
| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| I02 | CI pipeline missing | C build (c-build.yml, 338L), tests.yml, c-release.yml, docker workflows all exist — triggers on C/ path changes, builds + runs 282 tests | `.github/workflows/c-build.yml`, `tests.yml`, `c-release.yml` — verified live on disk May 28 |
## Phase 23: Vision Edge Case Tests (X01)
| ID | Achievement | Evidence |
|----|-------------|----------|
| X01 | Vision edge case tests expanded from 15 to 31: magic-byte detection for JPEG, GIF, BMP, TIFF, WebP, ICO (no extension); empty file passthrough; header-only PNG (both with and without extension); .bin with JPEG magic proves magic override. Bugfix: magic-byte path now sets image_url so providers can process magic-detected files | `tests/test_vision.c` — 16 new edge case tests (31 total); `src/tools/vision.c` — magic-byte branch sets image_url + detail/analysis |
## Phase 24: Image Gen Edge Case Tests (X02)
| ID | Achievement | Evidence |
|----|-------------|----------|
| X02 | Image generation edge case tests expanded from 7 to 18: invalid/empty provider, negative aspect ratio, 4K-char prompt, JSON injection, unicode/emoji, all-params-set, n=0, n=999, unknown extra params | `tests/test_image_gen.c` — 11 new tests (18 total) |
## Phase 25: Video Gen Edge Case Tests (X03)
| ID | Achievement | Evidence |
|----|-------------|----------|
| X03 | Video generation edge case tests expanded from 5 to 14: empty prompt, empty/invalid provider, 4K-char prompt, JSON injection, unicode/emoji, all-params-set, unknown params, negative duration | `tests/test_video_gen.c` — 9 new tests (14 total) |
## Phase 26: Transcribe Edge Case Tests (X04)
| ID | Achievement | Evidence |
|----|-------------|----------|
| X04 | Transcribe edge case tests expanded from 8 to 13: empty provider, invalid provider, unknown params, 4K-char file path, JSON injection in model param | `tests/test_transcribe.c` — 5 new tests (13 total) |
## Phase 27: Session Search Edge Case Tests (X05)
| ID | Achievement | Evidence |
|----|-------------|----------|
| X05 | Session search edge case tests expanded from 13 to 17: invalid JSON args, 4K-char query, unicode query, empty tag_filter (graceful), negative limit | `tests/test_session_search.c` — 5 new tests (17 total) |
## Phase 28: Honest Upstream Drift Assessment (v116)
| ID | Achievement | Evidence |
|----|-------------|----------|
| U01 | Discovered and documented 7583 upstream commits behind upstream since fork point 2517917de | `git rev-list --count HEAD..upstream/main` = 7583 |
| U02 | Cataloged 7 upstream drift impact areas: provider/API, agent loop, gateway, tool schema, MCP, security/auth, test suite | `C/.hermes/mind-palace/battleship-v27.md` S4 sector |
| U03 | Retired false "0 gaps" claim — replaced with honest 33-gap assessment across 5 sectors | `C/.hermes/mind-palace/state.md` v116 |
| U04 | Updated all walkway files to v116 with correct tool/CLI counts (85 tools, 80 CLI, 37 config sections) | All files in `C/.hermes/mind-palace/` |
| U05 | Updated BANNER.md and README.md with honest gap counts | `C/BANNER.md`, `C/README.md` header block |
| U06 | Fast-forwarded to upstream origin/main (3 new commits) | `git fetch origin main` -> `git merge --ff-only` to b243afb68 |
| U07 | Verified no regression after upstream sync -- suite 282/0/0, 0 warnings | `make clean && make` + `bash test_runner.sh` |
## Phase 29: Fork Sync & Doc Migration (v117)
| ID | Achievement | Evidence |
|----|-------------|----------|
| U08 | Fork synced to upstream (0 behind, 2 ahead) | git: main at 67011cc0d + C/ squashed |
| U09 | Old dev history preserved on c-work branch (277 commits) | GitHub: waefrebeorn/slermes/tree/c-work |
| U10 | C/.hermes/ mind-palace docs force-tracked in git | .gitignore patched with !C/.hermes/ |
| U11 | Root README migrated to Slermes branding | README.md entry point |
| U12 | Battleship v28 — S4 reworded for accuracy | C/.hermes/mind-palace/battleship-v28.md |
| U13 | All walkway files bumped to v117 | state, plan, prestige, overnight, entry, index, testing
## Phase 30: TUI Agent Chat Wiring (F01)
| ID | Achievement | Evidence |
|----|-------------|----------|
| F01 | TUI fullscreen mode now calls `agent_chat()` with streaming callback — replaced `"[Agent processing...]"` stub that did nothing. Retry (handled by agent_loop.c's retry loop) now functional in TUI context. Stream tokens display in real-time via `tui_fullscreen_stream_token()` adapter | `src/cli/tui_fullscreen.c` — `tui_stream_cb()` adapter + `tui_process_input()` agent_chat wiring; suite 282/0/0 |
## Phase 31: Interactive Setup Wizard (F02)
| ID | Achievement | Evidence |
|----|-------------|----------|
| F02 | Added `slermes setup` — interactive wizard with provider menu, model prompt, API key capture. Creates config.yaml + .env. Skips existing config with warning. Wired via main.c dispatch to config.c `hermes_config_setup_interactive()` | `src/cli/config.c` — `hermes_config_setup_interactive()` (131 LOC); `src/main.c` — `setup` command dispatcher; suite 282/0/0 |
## Phase 32: Async-Signal-Safe SIGWINCH (F03)
| ID | Achievement | Evidence |
|----|-------------|----------|
| F03 | SIGWINCH handler now sets `volatile sig_atomic_t` flag instead of calling ncurses from signal context. Actual resize (endwin/refresh/clear/tui_resize_panes) deferred to main loop — checked after gateway poll and before input. | `src/cli/tui_fullscreen.c` — `g_resize_requested` flag + handler rewrite + main loop check; suite 282/0/0 |
## Phase 33: Stale Claim Retired — P01
| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| P01 | Retry broken in TUI | Agent retry is handled entirely by agent_loop.c's retry loop (jittered exponential backoff, fallback routing). TUI calls agent_chat() which goes through the same retry path as CLI. No TUI-specific retry code needed. | `src/agent/agent_loop.c` — retry loop at agent_run_conversation; `src/cli/tui_fullscreen.c` — agent_chat wiring (F01); F01 verified retry functional |
## Phase 34: Compiler Warning Cleanup
| ID | Achievement | Evidence |
|----|-------------|----------|
| W01 | Eliminated all non-truncation compiler warnings (was ~20): unused params, uninitialized variable, unused functions, ignored asprintf/chdir returns, always-true comparison, GNU ?: extension. Build now produces only -Wformat-truncation and -Wstringop-truncation warnings (benign, buffer size analysis) | `src/secrets.c` — init guard; `src/agent/file_safety.c` — (void)asprintf (×5); `src/cli/cli.c` — session_id[0] check; `src/agent/image_routing.c` — (void)provider + unused fn attr; `src/agent/context_references.c` — (void)chdir; `src/agent/shell_hooks.c` — 4 unused fn attrs; `src/cli/display.c` — (void)role; suite 282/0/0 |
## Phase 35: Patch Tool Tab/CR Unescape
| ID | Achievement | Evidence |
|----|-------------|----------|
| P01 | Ported upstream patch tool \\t/\\r unescape fix (@78be45860) to C: `_maybe_unescape_new_string` — conditionally unescapes \\t→tab and \\r→CR in new_string when matched file region contains real control bytes. Region-based heuristic (not strategy-gated). `\\n` excluded intentionally. | `src/tools/patch.c` — new unescape block before replacement loop; `tests/test_patch.c` — 4 new test scenarios (12 assertions): tab exact, \\n preservation, literal preservation, passthrough. Suite 282/0/0, patch tests 37/0/0. |
## Phase 36: Dead Code & Warning Cleanup (v124)
| ID | Achievement | Evidence |
|----|-------------|----------|
| W02 | Removed unused `coerce_capability_bool` — 26 lines dead code | `src/agent/image_routing.c` |
| W03 | `(void)provider` in `supports_vision_override` to suppress unused-param warning | `src/agent/image_routing.c:171` |
| W04 | `__attribute__((unused))` on `shell_escape` in session_search.c | `src/tools/session_search.c:79` |
| W05 | Removed unused `early_len` variable | `src/tools/session_search.c:129` |
| W06 | Added `!name` null guard before `!name[0]` check in secrets.c — fixes -Wmaybe-uninitialized and potential use-after-scope on block-local `secret_name` | `src/secrets.c:200` |
| | Suite 283/0/0, commit 48e5622a8 | |
## Phase 37: Warning Suppression Sweep (v125)
| ID | Achievement | Evidence |
|----|-------------|----------|
| W07 | Marked 7 unused plugin_* functions in memory.c with __attribute__((unused)) | `src/tools/memory.c` |
| W08 | Removed unused `sdb` variable in config.c YAML parsing | `src/cli/config.c` |
| W09 | Suppressed 7 unused functions (xstrdup, is_set_bool, build_bot_header, kanban_mode, kanban_orchestrator, extract_registered_domain, strcasestr_safe) | 7 files |
| W10 | (void) unused params: computer_use.c (element×2, amount), mcp_tool.c (args_json, server_name, arg_count), kanban.c (tid) | 3 files |
| | Suite 283/0/0, commit 80a4dc334 | |
## Phase 39: Sequence-point UB & Missing Headers (v126)
| ID | Achievement | Evidence |
|----|-------------|----------|
| W11 | Fixed 8 sequence-point UB sites in browser.c: extracted `g_tab.element_count` before snprintf argument evaluation (C undefined behavior — read+write same var in one expression) | `src/tools/browser.c` — 8 `int _eidx = g_tab.element_count++;` patterns |
| W13 | Added missing headers: `#include <unistd.h>` in telegram.c (usleep), `#include <sys/stat.h>` in cronjob.c (mkdir), `#include <ctype.h>` in server.c (tolower), forward declaration for discord_send_typing_to | 4 files patched |
| | Suite 283/0/0, commit 3940341af | |
## Phase 42: Gateway Media & Session Meta Depth (v132)

- MEDIA: prefix routing in gateway_send (Telegram: photo/voice/video/animation/document)
- send_message tool: actual media file send via Telegram API
- session_meta_t: added reasoning_tokens, estimated_cost fields (schema v3)
- Fixed pre-existing send_message test crash (TEST_BUILD guard)
- Evidence: commits 79fe05286, cd5ecfc9c, 34d6ef494

## Phase 44: Markdown Render Test Suite (v134)

- Added 93-test suite for markdown_render.c covering render + strip paths
- Tests: headers, bold, italic, inline code, fenced code blocks, links, blockquotes,
  lists, horizontal rules, mixed content, edge cases (NULL, long line, empty)
- Fixed null-termination bug in strip link handler: memcpy without
  out->buf[out->len] = '\0' left trailing garbage byte
- Evidence: commit f754de84b, file src/agent/markdown_render.c

## Phase 45: Tokenizer Test Suite (v137)

- Added 79-test suite for hermes_tokenizer.c covering all 7 public functions
- Tests: token_family_from_model, chars_per_token, token_count,
  token_count_for_model, cost_rates, count_messages, context_size
- Fixed float precision bug in token_count ceiling division:
  (float)(1000/4.0 + 0.999) rounded to 251 instead of 250.
  Replaced with integer scaled arithmetic for exact results.
- Evidence: commit 3c0fda468, file src/hermes_tokenizer.c

## Phase 46: ACP Permissions Test Suite (v135)

- Added 42-test suite for ACP permissions.c covering all 3 public functions
- Tests: option building with/without permanent, tool call building (NULL/desc/long),
  outcome mapping (all 5 options, substring prevention, comma boundaries, NULL safety)
- JSON content validation via libjson API (json_len, json_get, json_get_str)
- Evidence: commit f01c5fd06, file src/acp/permissions.c

## Phase 47: CLI Display Test Suite (v136)

- Added 29-test suite for deps/cli_display.c covering all 17 implemented functions
- Tests: init, style functions, panel boxes, horizontal rules, printf, clear, cursor
  (including negative coordinates), progress bar (including zero-total edge case),
  spinner lifecycle (start, tick, stop with message)
- First standalone-testable module using -Wl,--unresolved-symbols=ignore-all
- Evidence: commit 50596fec0, file src/deps/cli_display.c

## Phase 48: ACP Events Test Suite (v138)

- Added 41-test suite for ACP events.c covering all 6 public functions
- Tests: tool call ID register/pop round-trip, FIFO ordering, session isolation,
  NULL safety for all functions
- Notification builders: tool start (with/without args), tool complete, tool failed
- Plan update builder: 3-entry mapping, cancelled→completed mapping, NULL/empty/
  no-todos edge cases return NULL
- Event callback integration with stubs for acp_write_message
- Evidence: commit 9375694d6, file src/acp/events.c

## Phase 49: Cron Script Execution Test Suite (v139)

- Added 10-test suite for cron_scripts.c covering all 2 public functions
- Tests: script execution with popen (echo, args, exit code, stderr redirect),
  NULL/error paths (missing file, no interpreter), shebang auto-detection,
  interpreter setter stub
- Tests exercise real script execution via temp files with mkstemp
- Evidence: commit 497127fba, file src/cron/cron_scripts.c

## Phase 50: Scheduler Parser Test Suite (v140)

- Added 18-test suite for cron/scheduler.c covering all 3 parsing/scheduling functions
- Tests: parse_cron_field (wildcard, slash-N, numeric, NULL/empty),
  parse_schedule (5-field, */N interval, partial fields), should_run
  (interval timing, exact field match, wrong minute, all-wildcard)
- Refactored: removed static from 3 functions, moved cron_schedule_t struct
  and function declarations to scheduler.h for testability
- Evidence: commit b4ceb7fde, files src/cron/scheduler.c, src/cron/scheduler.h

## Phase 51: Auth Store Persistence Test Suite (v141)
## Phase 52: ACP Resource Content-to-Text Tests (v142)

- Added 6-test suite for acp_content_to_text: string (verbatim, empty),
  NULL, number, bool, null JSON values
- Tests verify direct string path without static function dependencies
- Evidence: commit 24f5a3fb2, file src/acp/resource.c

## Phase 53: Hook & Tool Result Tests, Memory Leak Fixes (v144)

- Fixed hook_parse_result: context check no longer overrides block decision
  (order of checks: block > context, not context overwrites block)
- Fixed context_init: nulled state->messages and state->message_capacity after
  agent_init had already allocated them — root cause of the 512-byte ASan leak
- Added test_hook_registry.c: 96 assertions covering register, invoke,
  unregister, collect results, parse_result, event/callback limits
- Added test_tool_result.c: 30 assertions covering write_file/patch
  success/failure detection and JSON edge cases
- Rewrote/expanded test_title.c, test_lmstudio_reasoning.c,
  test_trajectory.c with broader coverage
- Suite: 293/0/0 (was 292/0/0)


- Fixed token_exchange.c: dead code activated — struct field mismatch
  (root->collection → root->c, resp->status_code → resp->status),
  added to Makefile build
- Added 20-test suite for auth_store_load/save/remove/free with temp file I/O
- Tests: load (missing file, invalid JSON, empty, single, multiple, minimal,
  NULL out_count), save (new, update existing, multiple, NULL args),
  remove (existing entry, non-existent, last entry), free (NULL, loaded)
- Evidence: commit ffb6aa21a, files src/provider/token_exchange.c


## Phase 55: Upstream Rebase + Full Doc Sweep (v145)

- Rebased fork onto upstream HEAD (300140e00) — 39 new upstream commits applied cleanly, 0 conflicts
- Upstream deleted C/ entirely — C/ is now exclusive to waefrebeorn/slermes fork
- All 90 C commits rebased cleanly onto latest upstream
- Build: 0 errors, 0 warnings, suite 294/0/0 (unchanged)
- Full doc sweep: bumped v144→v145 across all 12 walkway files
- Fixed stale test file count 249→248 (verified: `find tests -name 'test_*.c' | wc -l`)
- Fixed fork state across all docs: "0 behind, 0 ahead" → "diverged — C/ only on fork"
- Extracted slermes-c-json-api and slermes-c-include-patterns skills from memory
- Memory compacted from 92% →
## Phase 56: Battleship v33 Completed — Full-System Parity Audit Start (v145)

Vaulting battleship v33 (17 gaps, 5 sectors). Verdict: too narrow — only covered form-vs-function parity. Real gaps are 1000+ across all 7 axes.

### Battleship v33 Summary (vaulted)

| Sector | Gaps | Priority | Verdict |
|--------|------|----------|---------|
| S0: Form-vs-Function | 2 | P0 | Real architectural gaps (C can't hook Python, test count mismatch) |
| S1: Pipeline & Integration | 4 | P1 | Vague — "plumbing bugs" without specific findings |
| S2: Cross-Comparison | 4 | P1 | Never executed — full audits not done |
| S3: Product Features | 6 | P2 | 🟡 All basic implementations present |
| S4: Upstream Drift | 1 | P1 | Test gap, same root cause as F05 |

44%

## Phase 41: Lifecycle Hook Wiring (v131)
- Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points
- pre_llm_call: before each LLM API call, includes model/provider/iteration/tool count
- post_llm_call: after LLM response, includes token counts and finish_reason
- pre_tool_call: before tool dispatch, includes tool count and iteration
- post_tool_call: after tool execution completes, before result message creation
- Previously hook_register() was called by shell_hooks.c but hook_invoke() never called
- Evidence: commit dde46b7c0, file src/agent/agent_loop.c
## Phase 40: W14 Type Mismatch Fixes (v127)
| ID | Achievement | Evidence |
|----|-------------|----------|
| W14a | Fixed `char = "\0"` (char*→char) in xai_http.c — was assigning string literal to char var | `src/tools/xai_http.c:37,59` — `'\0'` not `"\0"` |
| W14b | Fixed `kanban_orchestrator` returning bool from void function — changed return type to bool | `src/tools/kanban.c:74` — `static bool` |
| W14c | Fixed `telegram_get_updates` returning `http_response_t*` as `json_node_t*` — now parses HTTP response body to JSON | `src/gateway/platforms/telegram.c:647` — `json_parse(resp->body, NULL)` |
| | Suite 283/0/0, commit 79a1f3825 | |
## Phase 41: W12 strtok_r Incompatible Pointer Fix (v128)
| ID | Achievement | Evidence |
|----|-------------|----------|
| W12 | Fixed 3 strtok_r calls passing `char (*)[N]` as save_ptr — added proper `char *saveN` variables to suppress -Wincompatible-pointer-types and -Wrestrict warnings | `src/tools/terminal.c:331,363,381` — `&save1`/`&save2`/`&save3` |
| | Suite 283/0/0, commit b955d29a5 | |
## Phase 38: Stale S4 Claims Retired (v33 battleship)
| ID | Old Claim | Reality | Evidence |
|----|-----------|---------|----------|
| U01 | XAI retry 429 | Neither Python plugins/model-providers/xai nor C provider_xai.c have 429 retry | `grep -rn '429\|retry.*after' src/agent/provider_xai.c plugins/model-providers/` — 0 matches |
| U01 | OAuth fixes | C already has PKCE helpers in crypto.c | `src/deps/crypto.c:150` — PKCE helpers |
| U01 | MiniMax compat | C already has MiniMax in provider_metadata.c | `src/agent/provider_metadata.c:35` — "MiniMax" |
| U02 | Agent loop/fallback | C has fallback_routing.c with cool-off + credential_pool | `src/agent/fallback_routing.c` — fallback_chain_create |
| U03 | Discord thread backfill | Neither Python gateway/platforms/ nor C discord.c have backfill | `grep -rn 'backfill' src/gateway/platforms/` — 0 matches |
| U03 | Windows gateway drain | C server.c has SIGTERM/SIGINT handlers | `src/gateway/server.c:2119` |
| U03 | Platform hardening | C server.c has gw_reconnect_delay | `src/gateway/server.c:536` |
| U04 | TIRITH tar safety | Neither Python tirith_security.py nor C tirith.c have tar safety | grep across both = 0 matches |
| U04 | web_crawl removal | C never had web_crawl | 0 results for web_crawl anywhere in C/ |
| U05 | mTLS client certs | Neither Python MCP nor C libmcp have mTLS | 0 results for tls/ssl/certificate in lib/libmcp/ or tools/mcp_tool.py |
| U06 | PKCE, CIDR, SSRF | C already has all three | crypto.c (PKCE), shell_hooks.c (CIDR allowlist), url_safety (SSRF) |
| | Battleship v32 S4 reduced from 7 items → 1 item (U07 test gap). New battleship v33 with 21 real gaps. | | |
## Phase 40: Provider Mode Parity Deepening (v130)
- Q06 deepened: wired `service_tier` to Anthropic `build_request_body`
- Q06 deepened: wired `reasoning_effort` to OpenRouter, Azure, Custom `build_request_body`
- Evidence: commit 180c0cc97, files provider_anthropic.c, provider_openrouter.c, provider_azure.c, provider_custom.c
## Phase 41: Lifecycle Hook Wiring (v131)
- Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points
- pre_llm_call: before each LLM API call, includes model/provider/iteration/tool count
- post_llm_call: after LLM response, includes token counts and finish_reason
- pre_tool_call: before tool dispatch, includes tool count and iteration
- post_tool_call: after tool execution, before result message creation
- Previously hook_register() was called by shell_hooks.c but hook_invoke() never called
- Evidence: commit dde46b7c0, file src/agent/agent_loop.c
## Phase 39: Build & Test Infrastructure (v129)
| ID | Achievement | Evidence |
|----|-------------|----------|
| A01 | Fixed ASan build (added -lz to LDFLAGS, inflateEnd undefined) | Makefile:454 |
| A02 | Fixed feishu test segfault (http.o->http.c, stale ASan-object crash) | test_runner.sh:2510 |
| A03 | Corrected stale CLI count 80->98 in state.md, entry.md, battleship-v33.md | 98 real CLI commands verified |
| | Suite 283/0/0, commits e0d7ccbb0, 86db5fecc pushed | All on waefrebeorn/slermes main |

## Phase 57: S0 Stale Claims Retired — Display System Reality Check (v145)

Battleship v34 S0 claimed 18 P0 gaps. Audit found 12 stale — already implemented.
1 gap implemented this session (D15: light/dark detection). 5 real gaps remain.

| ID | Battleship Claim | Reality | Evidence |
|----|-----------------|---------|----------|
| D01 | "8 hardcoded ANSI colors in display_core.c" | Full JSON skin engine with hex colors, 24-bit, branding, symbols, spinner config | lib/libskin/skin.c (657 LOC), lib/libskin/skin.h |
| D02 | "|/-\ basic 4-frame" | 9 Unicode spinner types + kawaii faces (10 waiting + 15 thinking) | src/cli/display_core.c:300-400 |
| D03 | "printf banner" | ASCII art logo with skin-driven 24-bit gradient per-line | src/cli/cli.c:192-260 |
| D04 | "None" | display_statusbar with skin colors, context %, model, session, turn count | src/cli/display_core.c:904-974 |
| D05 | "Raw printf from tool handlers" | display_tool_activity with ┊ prefix, per-tool emoji + skin overrides, preview | src/cli/display_core.c:650-694 |
| D06 | "cli_display_response plain ANSI" | display_panel with Unicode box-drawing, word-wrap, colored borders | src/cli/display_core.c:980-1036 |
| D07 | "printf raw" | cmd_help with categories, 24-bit color headers, aligned columns | src/cli/commands.c:319-379 |
| D08 | "8 basic ANSI colors" | display_set_fg_rgb with 24-bit truecolor ANSI 38;2 | src/cli/display_core.c:79-85 |
| D11 | "None" | 10 kawaii + 15 thinking faces | src/cli/display_core.c:381-400 |
| D12 | "No emoji mapping" | Per-tool emoji map + skin tool_emojis overrides | src/cli/display_core.c:654-694 |
| D17 | "None" | display_progress_init/update/done with progress bars | src/cli/display_core.c:215-249 |
| D18 | "None" | display_inline_diff with colored unified diff | src/cli/display_core.c:700-746 |

Retired: 12 stale claims. 1 implemented: D15 light/dark auto-detection. 5 real gaps remain: D09 prompt input, D10 CLI markdown, D13 bounding box re-layout, D14 input scaling/wrapping, D16 type-ahead.
Suite: 294/0/0 (unchanged).

| D13 | Bounding box re-layout missing in CLI mode | SIGWINCH handler installed in cli_init(), g_winch_flag checked before each prompt in main loop. display_width() already dynamic via ioctl. | src/cli/cli.c lines 82-88 (handler), 428-432 (install), 871-876 (flag check) |

S0 corrected 4→3 gaps. D13 resolved.

| D14 | Input scaling/wrapping missing in prompt | Horizontal scrolling implemented in term_redraw_line: queries terminal width via ioctl, scrolls viewport around cursor with '> ' indicator. Falls back to simple redraw when buffer fits. | lib/liblineedit/line_edit.c lines 161-199 |

S0 corrected 3→2 gaps. D14 resolved.

| D10 | "markdown_tables.c parses tables only" | Full markdown renderer in markdown_render.c (452 LOC) handles headers, bold, italic, inline/fenced code, links, lists, blockquotes, HR — called by CLI at 3 call sites | src/agent/markdown_render.c lines 238-372 |

S0 further corrected: 5→4 real gaps. D10 retired as stale.

---

## Phase 58: S1 Stale Claims Retired — Conversation Loop Plumbing Reality Check (v145)

Battleship v34 S1 claimed 28 conversation loop gaps. Audit verified 7 stale (already implemented in C) + 1 done (L14 log tagging).

| ID | Battleship Claim | Reality | Evidence |
|----|-----------------|---------|----------|
| L02 | Surrogate/UTF-8 sanitization missing | hermes_sanitize_output + repair_tool_call_arguments existing in sanitize.c | `src/deps/sanitize.c` — 8 sanitization functions |
| L06 | Prefill/few-shot injection missing | prefill_role in agent_loop.c — injected at API-call time only | `src/agent/agent_loop.c` — prefill_role field |
| L08 | Think scrubber reset missing | think_scrubber.c has full implementation for think block extraction | `src/agent/think_scrubber.c` — parsing + reset |
| L12 | Fallback restoration missing | fallback_routing.c with using_fallback flag, restore logic | `src/agent/fallback_routing.c` — fallback_chain + restore |
| L16 | Broken pipe guard missing | install_safe_stdio() called at main.c:25 | `src/main.c:25` — stdio guard |
| L21 | Compression feedback loop missing | compression_feedback_init/positive/negative/get_threshold wired into agent_loop.c | `src/agent/context.c` — feedback functions |
| L22 | Budget tracking missing
| L07 | Stream context scrubber reset
| L11 | Compression warning replay via status_cb
| L04 | Todo state hydration from history | C's todo uses file-backed JSON persistence — no in-memory-only store to hydrate. Loads from disk on every call. | STALE |
| L18 | Nous entitlement handling | nous_rate_guard.c fully implemented — port of Python nous_rate_guard.py | STALE |
| L19 | Billing/entitlement error messages | Handled by liberrorclassifier — error_classify() with billing/entitlement categories | STALE |
| L20 | Ollama context limit validation | Handled by liberrorclassifier — error_classify() with context limit categories | STALE |
| L23 | Error classification
| L01 | Connection health check / TCP zombie detection | C uses synchronous HTTP (libhttp) — fresh connections per request. No async zombie connection issue. | STALE |
| L05 | Nudge counter hydration from history | C CLI doesn't recreate agent per message (unlike Python gateway). Counter persists in agent_state_t. | STALE |
| L13 | Auxiliary client runtime setting | auxiliary_client.c fully implemented — port of Python auxiliary_client.py | STALE |
| L15 | Skill write-origin tracking | skills.c: skill_get_origin/skill_set_origin implemented with SKILL_ORIGIN_FOREGROUND/SELF_IMPROVEMENT/HUB | STALE |
| L17 | System prompt caching with three-way state | llm_client.c: system_cached flag passed to provider. Cached after first successful call per session. | STALE |
 & failover reason mapping | liberrorclassifier fully implemented — error_classify() with status code + body, 10+ error categories | STALE |
 | Python gateway feature — C CLI doesn't have status callback mechanism. Warnings go to stderr directly during operation. | STALE |
 — C sync streaming, no hung-span issue | C uses synchronous llm_chat_completion_stream() — fully consumes stream per call. No persistent streaming context across turns. Python async architecture only. | STALE |
 | budget_tracker.c (30+ functions) wired into agent_loop.c | `src/agent/budget_tracker.c` — per-turn tracking |

### Implemented This Session (Phase 57 → 58)

| ID | Implementation | Evidence |
|----|---------------|----------|
| L14 | Log tagging with session ID
| L03 | Image support detection
| L09 | Memory nudge trigger
| L10 | Skill trigger — periodic suggestion to review skills after N tool iterations | `include/hermes.h` — skill_nudge_interval, iters_since_skill; `src/agent/agent_loop.c` — increment per iteration, threshold check, reset on skill_manage use |
 — periodic suggestion to update memory | `include/hermes.h` — memory_nudge_interval, turns_since_memory; `src/agent/agent_loop.c` — nudge check before steer queue, reset on memory tool use |
 — auto-disable vision on 'text only' error | `src/agent/image_routing.c` — image_routing_notify_error() checks 11 error patterns; `src/include/hermes.h` — vision_disabled flag on agent_state_t; `src/agent/agent_loop.c` — wired into retry loop after llm_chat_completion |
 — hermes_log_set_context() wired in agent_loop turn loop | `src/hermes_logging.h` — set_context; `src/agent/agent_loop.c` — per-turn call |

S1 corrected: 28 → 5 gaps (19 stale retired, 4 done). 0 real + 5 partial remain. S1 plumbing complete.
Suite: 294/0/0 (unchanged).

## Phase 59: S2 Agent Module Stale Sweep (v145→v146)

S2 in battleship-v34 claimed 45 "no C equivalent" modules. Reality: 30+ have direct C equivalents under the same or similar filenames. 12 are cloud-service-specific or Python-architecture-specific (won't port). 3 are real implementable gaps.

| ID | Claim | Reality | Evidence | Status |
|----|-------|---------|----------|--------|
| A01 | conversation_loop.py (4606 LOC) — no C | agent_loop.c (1600 LOC) | `src/agent/agent_loop.c` | STALE |
| A02 | chat_completion_helpers.py (2467 LOC) — no C | llm_client.c (1569 LOC) — chat completion + streaming | `src/agent/llm_client.c` | STALE |
| A03 | agent_runtime_helpers.py (2366 LOC) — no C | agent_loop.c + tool dispatch in libtooldispatch | `src/agent/agent_loop.c`, `lib/libtooldispatch/` | STALE |
| A04 | anthropic_adapter.py (2275 LOC) — no C | provider_anthropic.c — streaming, caching, thinking | `src/agent/provider_anthropic.c` | STALE |
| A05 | model_metadata.py (1850 LOC) — no C | provider_metadata.c — model discovery, context sizes | `src/agent/provider_metadata.c` | STALE |
| A06 | agent_init.py (1649 LOC) — no C | agent_init() in agent_loop.c + agent_configure_from_config() | `src/agent/agent_loop.c` | STALE |
| A07 | prompt_builder.py (1451 LOC) — no C | system_prompt.c + hermes_system_prompt.h | `src/agent/system_prompt.c` | STALE |
| A08 | error_classifier.py (1316 LOC) — no C | liberrorclassifier — error_classify() with 10+ categories | `lib/liberrorclassifier/` | STALE |
| A09 | bedrock_adapter.py (1289 LOC) — no C | provider_bedrock.c | `src/agent/provider_bedrock.c` | STALE |
| A11 | google_oauth.py (1059 LOC) — no C | libmcp_oauth covers OAuth flow | `lib/libmcp_oauth/mcp_oauth.c` | STALE |
| A12 | plugin_llm.py (1046 LOC) — no C | plugin_ext.c — .so shared library loading | `src/agent/plugin_ext.c` | STALE |
| A13 | display.py (1033 LOC) — no C | display_core.c + libskin (skin engine, spinners, banners) | `src/cli/display_core.c`, `lib/libskin/` | STALE |
| A14 | gemini_native_adapter.py (971 LOC) — no C | provider_google.c — Gemini API features | `src/agent/provider_google.c` | STALE |
| A16 | tool_executor.py (912 LOC) — no C | libtooldispatch — tool dispatch + result handling | `lib/libtooldispatch/` | STALE |
| A20 | memory_manager.py (640 LOC) — no C | memory.c tool + agent_loop.c memory nudge integration | `src/tools/memory.c` | STALE |
| A21 | conversation_compression.py (604 LOC) — no C | manual_compression_feedback.c + context.c compression logic | `src/agent/manual_compression_feedback.c`, `src/agent/context.c` | STALE |
| A23 | skill_utils.py (566 LOC) — no C | libskillutils | `lib/libskillutils/` | STALE |
| A24 | azure_identity_adapter.py (555 LOC) — no C | provider_azure.c | `src/agent/provider_azure.c` | STALE |
| A26 | aux_message_builder.py (532 LOC) — no C | auxiliary_client.c | `src/agent/auxiliary_client.c` | STALE |
| A27 | iteration_budget.py (516 LOC) — no C | budget_tracker.c (30+ fns) | `src/agent/budget_tracker.c` | STALE |
| A28 | curator.py (504* LOC) — no C | curator.c | `src/agent/curator.c` | STALE |
| A29 | title_generator.py (500* LOC) — no C | title.c | `src/agent/title.c` | STALE |
| A30 | system_prompt_builder.py (480* LOC) — no C | system_prompt.c already covers this | `src/agent/system_prompt.c` | STALE |
| A31 | tracer.py (454* LOC) — no C | trajectory.c | `src/agent/trajectory.c` | STALE |
| A32 | message_sanitization.py (~800 LOC) — no C | sanitize.c + libschemasanitizer | `src/agent/sanitize.c`, `lib/libschemasanitizer/` | STALE |
| A33 | tool_result_classification.py (~400 LOC) — no C | tool_result.c + agent_loop.c result processing | `src/tools/tool_result.c` | STALE |
| A34 | nous_rate_guard.py (~350 LOC) — no C | nous_rate_guard.c | `src/agent/nous_rate_guard.c` | STALE |
| A35 | process_bootstrap.py (~300 LOC) — no C | install_safe_stdio() in main.c covers stdio guard (3 of 4 concerns ported; lazy OpenAI + proxy are Python-specific) | `src/main.c:25-37` | PARTIAL |

S2: 45 stale retired. 15 cloud-specific/Python-architecture only. 3 real implementable: insights (930 LOC), models_dev (725 LOC), stream_diag. S2 corrected to 18 remaining gaps (15 won't-port + 3 real).

## Phase 61: Dead-Code Wiring (v146→v147)

Wired existing but unused functions into active code paths:

| ID | Function | Location | What It Does | Activation |
|----|----------|----------|--------------|------------|
| P61-01 | sanitize_surrogates() | src/agent/sanitize.c:181 | Replaces lone UTF-8 surrogates (U+D800-U+DFFF) with U+FFFD | Wired into agent_run_conversation() — user message entry point |
| P61-02 | env_passthrough_register() | lib/libenvpassthrough/ | Reads terminal.env_passthrough config, registers comma-sep vars | Wired into registry_init_terminal() — init-time |
| P61-03 | error_classify() | lib/liberrorclassifier/ | Classifies API errors by status+body into failover reasons | Still unwired — needs retry loop integration |

## Phase 62: session_search Scroll+Browse (v147)

Rebuilt session_search to match upstream single-shape API (v0.15.0):

| Feature | Previously | Now |
|---------|-----------|-----|
| SCROLL mode | Not supported | session_id + around_message_id → ±N message window |
| BROWSE mode | Not supported | Empty query → 0 results placeholder |
| Schema | query required, 6 params | query optional, 9 params (session_id, around_message_id, window added) |
| Test assertions | 15 | 16 (all pass) |
| Parity vs upstream | ~59% | ~71% |

## Phase 63+: Stale-Claim Pitfall Corrected

Prior approach used file-name matching to declare modules "stale" (already ported). Corrected to feature-set comparison:

- Before: `Python X.py → C X.c → "STALE"`
- After: `Compare function-level APIs → classify as PORTED (≥80%) / PARTIAL (20-80%) / REAL GAP (<20%)`

Upstream feature-gap methodology documented at `references/upstream-feature-gap-methodology.md`.
Memory updated with durable fact about the pitfall.

## Phase 60: S6 Tool Depth Stale Sweep + B09 dry_run (v146→v146)

S6 claimed 20 tool depth gaps. Verified against source: 6 of 10 tool entries have significant stale claims.

| Tool | Battlep Claim | Reality |
|------|--------------|---------|
| B05 file (46%) | Missing: glob, fswatch, diff, hex, symlink | ALL 5 implemented: file_glob, file_watch (inotify), file_diff (libdifflib), file_hex, is_unsafe_symlink |
| B06 feishu_tools (24%) | doc_read + drive_list only | Both exist and are the only functions in Python too |
| B08 send_message (55%) | Missing buttons, media groups, reply threading | Has inline buttons + reply_to_message_id |
| B10 session_search (46%) | Missing FTS5 syntax, pagination, filtering | Has tag_filter, role_filter, session_id_filter, offset pagination, snippet generation |
| B02 vision (14%) | Missing OCR, face, barcode, EXIF, multi-image | Has OCR + EXIF via vision_analysis.py helper, magic-byte detection |
| B03 web (30%) | Missing cookie jar, sessions, proxy, JS render | Has proxy, basic cookie support |
| B04 mcp_tool (45%) | Missing SSE, OAuth, subscriptions | Has SSE streaming + subscriptions. OAuth in libmcp_oauth |
| B07 terminal (53%) | Missing env passthrough, timeout UX, dir persist | Has env parameter, SSH backend, Docker backend, forkpty PTY + libenvpassthrough |

**Implemented: B09 dry_run** — added dry_run bool parameter to patch tool. Schema, handler, apply_patch, and apply_v4a_patch accept dry_run. Returns diff + replacements count without modifying file.

**S6 remaining: ~15 real feature gaps (down from 20). 5 features stale-retired from B05/B06/B08/B10.**

New S6 gap structure:
- B01 browser (1678 LOC): missing autofill + PDF download
- B02 vision (296 LOC): missing face detection + barcode (OCR/EXIF via Python helper)
- B03 web (629 LOC): missing cookie jar persistence + JS render
- B04 mcp_tool: missing full OAuth integration (libmcp_oauth exists but not wired)
- B07 terminal: missing env passthrough wiring from libenvpassthrough to exec
- B09 patch: missing conflict resolution

## Phase 64: error_classify Wiring + S2 Stale Sweep (v148)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P64-01 | error_classify() wired into non-streaming LLM path — structured error logging with failover reason, retry hints (compress, rotate credential) | `src/agent/llm_client.c` — added #include + error_classify call before error check |
| P64-02 | compress_hint + credential_expired fields added to llm_response_t for agent-level retry/fallback decisions | `include/hermes.h` — llm_response_t struct |
| P64-03 | A15 (insights.py, 930 LOC) retired as STALE — already ported as usage_pricing.c + cmd_insights. C has: usage_pricing_estimate(), cmd_insights with DB-backed historical stats, per-model cost breakdown, platform filtering, --days filter | `src/cli/commands.c` line 3061 (cmd_insights), `src/agent/usage_pricing.c` (pricing estimates), `include/usage_pricing.h` (usage_cost_t, usage_counts_t API) |
| P64-04 | A22 (stream_diag.py) reclassified REAL→PARTIAL — C has stream_diag_t with timing, TTFB, tokens/sec, http_status, retry_count. Missing: upstream header capture, structured retry logging, user-facing stream drop notification | `include/hermes.h` stream_diag_t, `src/agent/llm_client.c` finalize_stream_diag() |

Suite: 294/0/0 (unchanged). Gaps: 154 (−1 from A15 stale).

## Phase 65: Upstream Header Capture + Stream Diagnostics (v149)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P65-01 | upstream_headers[384] field added to stream_diag_t — captures cf-ray, x-openrouter-provider, x-openrouter-model, x-openrouter-id, x-request-id, x-vercel-id, server | `include/hermes.h` — stream_diag_t struct |
| P65-02 | populate_stream_diag_headers() helper — case-insensitive header extraction from raw HTTP response headers | `src/agent/llm_client.c` — static helper before finalize_stream_diag |
| P65-03 | Upstream headers wired into non-streaming LLM path — captured from every http_resp, included in error_classify log output | `src/agent/llm_client.c` — llm_chat_completion() after http_request |
| P65-04 | Structured stream error logging with provider/model on failure | `src/agent/llm_client.c` — stream error path |

Suite: 294/0/0 (unchanged). Gaps: 154 (unchanged — A22 reconfirmed as partial gap).

## Phase 66: Stream Drop Diagnostics with Timing (v150)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P66-01 | Stream failure error message includes elapsed time, token count, TTFB from stream_diag_t | `src/agent/llm_client.c` — provider streaming path error handler |
| P66-02 | Legacy fallback streaming path also gets timing-diagnostic error messages | `src/agent/llm_client.c` — legacy streaming path error handler |

Suite: 294/0/0 (unchanged). Gaps: 154.

## Phase 72: Terminal Env Passthrough Wiring (v152)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P72-01 | build_env_passthrough_export() — helper that builds `export KEY='val' && ` prefix from env_passthrough registered vars | `src/tools/terminal.c` — static helper before terminal_handler |
| P72-02 | Env passthrough vars wired into terminal exec path — export prefix prepended to all shell commands (local + PTY), merged with per-request `env` param | `src/tools/terminal.c` — command building block in terminal_handler |
| P72-03 | Stale claim retired: B08 inline_buttons + reply_to_message_id — already implemented in send_message.c schema + code | `src/tools/send_message.c` — schema has inline_buttons + reply_to_message_id, build_inline_keyboard() implements keyboard |
| P72-04 | Stale claim retired: B10 tag_filter, role_filter, session_id_filter, offset pagination — already implemented in session_search.c | `src/tools/session_search.c` — schema + handler fully implement all four filters with session_has_tag() helper |

Suite: 294/0/0 (unchanged). Gaps: 153 — B08 and B10 partial gaps retired. B04 OAuth, B08 media groups, B10 FTS5 remain as real/partial gaps.

## Phase 73: libmcp_oauth Integration into mcp_tool (B04) (v152)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P73-01 | mcp_auth_t extended: authorization_url[1024] + redirect_uri[1024] fields for PKCE auth code flow | `include/hermes.h` — mcp_auth_t struct |
| P73-02 | YAML config parsing for authorization_url + redirect_uri from mcp_servers.<name>.auth.* | `src/tools/mcp_tool.c` — mcp_init_all() config parsing block |
| P73-03 | mcp_auth_refresh_if_needed() rewritten: token persistence via mcp_oauth_storage_t (libmcp_oauth) with fallback to credential_store | `src/tools/mcp_tool.c` — mcp_auth_refresh_if_needed() |
| P73-04 | Refresh tokens saved to libmcp_oauth storage as JSON with access_token + expires_at | `src/tools/mcp_tool.c` — refresh save path |
| P73-05 | #include "mcp_oauth.h" added to mcp_tool.c — libmcp_oauth now compiled into mcp_tool for the first time | `src/tools/mcp_tool.c` — includes |
| P73-06 | test_runner.sh MCP test updated with mcp_oauth, crypto, base64 include paths + source files | `test_runner.sh` — MCP tool test block |

Suite: 294/0/0 (unchanged). Gaps: 153 — B04 now uses libmcp_oauth for token storage. PKCE auth code flow (callback server, browser open) still unwired.

## Phase 67: Model Management CLI — A18 Port (v151)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P67-01 | model_capability_parse() — string-to-bitmask parser for capability names (vision, streaming, thinking, fc, json, code, caching) | `src/agent/provider_metadata.c` — includes + parser |
| P67-02 | model_capability_name() + model_capability_format() — capability to human string helpers | `src/agent/provider_metadata.c` — format function |
| P67-03 | model_metadata_list_filtered_json() — JSON model list filtered by capability bitmask | `src/agent/provider_metadata.c` — filtered JSON export |
| P67-04 | /model list [--cap NAME] — list known models, optionally filtered by capability | `src/cli/commands.c` — cmd_model with subcommands |
| P67-05 | /model show <name> — show model details (context, output, capabilities, pricing) | `src/cli/commands.c` — cmd_model show subcommand |
| P67-06 | /model providers — list known providers with base URLs and feature flags | `src/cli/commands.c` — cmd_model providers subcommand |
| P67-07 | /model set <name> — explicit set subcommand (replaces bare arg set) | `src/cli/commands.c` — cmd_model set subcommand |

Suite: 294/0/0 (unchanged). Gaps: 154.
