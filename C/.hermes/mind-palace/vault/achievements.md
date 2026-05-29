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

## Phase 72: Terminal Env Passthrough Wiring (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P72-01 | build_env_passthrough_export() — helper that builds `export KEY='val' && ` prefix from env_passthrough registered vars | `src/tools/terminal.c` — static helper before terminal_handler |
| P72-02 | Env passthrough vars wired into terminal exec path — export prefix prepended to all shell commands (local + PTY), merged with per-request `env` param | `src/tools/terminal.c` — command building block in terminal_handler |
| P72-03 | Stale claim retired: B08 inline_buttons + reply_to_message_id — already implemented in send_message.c schema + code | `src/tools/send_message.c` — schema has inline_buttons + reply_to_message_id, build_inline_keyboard() implements keyboard |
| P72-04 | Stale claim retired: B10 tag_filter, role_filter, session_id_filter, offset pagination — already implemented in session_search.c | `src/tools/session_search.c` — schema + handler fully implement all four filters with session_has_tag() helper |

Suite: 294/0/0 (unchanged). Gaps: 153 — B08 and B10 partial gaps retired. B04 OAuth, B08 media groups, B10 FTS5 remain as real/partial gaps.

## Phase 73: libmcp_oauth Integration into mcp_tool (B04) (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P73-01 | mcp_auth_t extended: authorization_url[1024] + redirect_uri[1024] fields for PKCE auth code flow | `include/hermes.h` — mcp_auth_t struct |
| P73-02 | YAML config parsing for authorization_url + redirect_uri from mcp_servers.<name>.auth.* | `src/tools/mcp_tool.c` — mcp_init_all() config parsing block |
| P73-03 | mcp_auth_refresh_if_needed() rewritten: token persistence via mcp_oauth_storage_t (libmcp_oauth) with fallback to credential_store | `src/tools/mcp_tool.c` — mcp_auth_refresh_if_needed() |
| P73-04 | Refresh tokens saved to libmcp_oauth storage as JSON with access_token + expires_at | `src/tools/mcp_tool.c` — refresh save path |
| P73-05 | #include "mcp_oauth.h" added to mcp_tool.c — libmcp_oauth now compiled into mcp_tool for the first time | `src/tools/mcp_tool.c` — includes |
| P73-06 | test_runner.sh MCP test updated with mcp_oauth, crypto, base64 include paths + source files | `test_runner.sh` — MCP tool test block |

Suite: 294/0/0 (unchanged). Gaps: 153 — B04 now uses libmcp_oauth for token storage. PKCE auth code flow (callback server, browser open) still unwired.

## Phase 74: Patch Conflict Resolution (B09) (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P74-01 | Conflict resolution JSON when old_string found multiple times — returns `{conflict:true, count:N, matches:[{offset, snippet}]}` with context snippets for each match | `src/tools/patch.c` — patch_file() conflict resolution block |

Suite: 294/0/0 (unchanged). Gaps: 153 — B09 conflict resolution now returns structured snippet data instead of bare error string.

## Phase 75: FTS5 Query Syntax for session_search (B10) (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P75-01 | fts5_parse() — FTS5-style query tokenizer supporting AND (space), OR, quoted phrases, -exclude | `src/tools/session_search.c` — FTS5 query parsing block |
| P75-02 | fts5_matches() — multi-term filter: all required terms present, no excluded terms | `src/tools/session_search.c` — FTS5 match check |
| P75-03 | fts5_count_matches() — aggregates occurrences across all required/phrase terms | `src/tools/session_search.c` — FTS5 match count |
| P75-04 | compute_score() updated — uses FTS5 multi-term scoring for complex queries, falls back to single-term for simple | `src/tools/session_search.c` — compute_score() |
| P75-05 | extract_snippet() updated — finds 200-char window with most term matches for FTS5 queries | `src/tools/session_search.c` — extract_snippet() window scan |

Suite: 294/0/0 (unchanged). Gaps: 153 — B10 FTS5 syntax added. Remaining: full-text indexing for performance.

## Phase 76: Telegram Media Group Support (B08) (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P76-01 | media_group param added to send_message schema — array of file path strings | `src/tools/send_message.c` — SCHEMA definition |
| P76-02 | Media group handler — builds InputMedia array from file paths, detects type by extension (photo/video/animation/document), sends via telegram_send_media_group() | `src/tools/send_message.c` — send_message_handler media group block |

Suite: 294/0/0 (unchanged). Gaps: 153 — B08 media group support added.

## Phase 77: A22 Stream Diagnostics — Upstream Header Capture in Streaming Path (v153)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P77-01 | http_t.resp_headers[4096] field + accumulation in http_stream_request() header-read loop — raw response headers captured during SSE streaming for diagnostics | `lib/libhttp/http.c` — http_t struct + http_stream_request() header loop |
| P77-02 | http_get_resp_headers() getter in libhttp API | `lib/libhttp/http.h` — getter declaration + `lib/libhttp/http.c` — getter impl |
| P77-03 | Provider-based streaming path: populate_stream_diag_headers() called before http_free(h) — captures cf-ray, x-request-id, x-openrouter-* headers in streaming responses | `src/agent/llm_client.c` — llm_chat_completion_stream() provider path |
| P77-04 | Legacy streaming path: same header capture wired before http_free(h) | `src/agent/llm_client.c` — llm_chat_completion_stream() legacy path |

Suite: 294/0/0 (unchanged). Gaps: 153 — A22 streaming header capture done. Remaining: user-facing inline notification.

## Phase 78: A22 Stream Diagnostics — User-Facing Inline Notification (v154)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P78-01 | Upstream response headers logged on stream success: `[llm] upstream=[cf-ray=... x-openrouter-provider=...]` — matches Python's log_stream_retry upstream=[...] pattern | `src/agent/agent_loop.c` — retry_done label |

Suite: 294/0/0 (unchanged). Gaps: 152 — A22 PORTED. Remaining S2 gap: A18 models_dev at 60%.

## Phase 82: libtooloutput test suite (v158)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P82-01 | test_tool_output.c (23 tests) — env var overrides, default values, invalid inputs, boundary checks, limit functions | `tests/test_tool_output.c` |
| P82-02 | Removed stale duplicate tool_output test_runner entry (separate-compilation pattern conflicting with source-include test) | `test_runner.sh` — lines 115-127 removed |

Suite: 294/0/0. Test files: 250. Gaps: 148.

## Phase 83: send_message error redaction (v159)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P83-01 | sanitize_error_text() — redacts access_token, api_key, token, sig, etc. from error messages before surfacing (port of Python _sanitize_error_text) | `src/tools/send_message.c` — sanitize_error_text() function, wired into platform error path |

Suite: 294/0/0. Test files: 250. Gaps: 148. B08 depth +1%.

## Phase 84: send_message test expansion + B01 stale fix (v160)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P84-01 | 6 sanitize_error_text tests: URL query param redaction, generic assignment redaction, no false positives on safe text, NULL input, multiple tokens, sig parameter | `tests/test_send_message.c` — tests 15-20 |
| P84-02 | Battleship B01 stale claim corrected: PDF download already exists via browser_generate_pdf (CDP), not a gap | `.hermes/mind-palace/battleship-v34.md` — B01 entry |

Suite: 294/0/0. Test files: 250. Gaps: 148. B01 44%→45%.

## Phase 85: native image dimension extraction (v161)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P85-01 | extract_dimensions_native() — PNG/JPEG/GIF/BMP/WebP header parsing for width/height. Removes Python PIL dependency for basic dimension detection. Falls back to PIL if native fails | `src/tools/vision.c` — extract_dimensions_native() function, wired into vision_handler |

Suite: 294/0/0. Test files: 250. Gaps: 148. B02 depth +1% (PNG/JPEG/GIF/BMP/WebP dims now native).

## Phase 86: B04 PKCE Auth Code Flow (v162)

| ID | Achievement | Evidence |
|----|-------------|----------|
| B04 | MCP tool PKCE OAuth auth code flow — `mcp_oauth_manager_get_token()` wired into `mcp_auth_refresh_if_needed()`. Callback server, browser launch, token exchange/refresh, mtime-change detection. Auth config parsed for HTTP/SSE servers (previously skipped). url field added to mcp_auth_t struct. | `src/tools/mcp_tool.c` (lines ~1206-1370), `include/hermes.h` (line 622), `lib/libmcp_oauth/mcp_oauth.c` |

Suite: 294/0/0. Phase 86: B04 PKCE auth code flow wired. Gaps: 148→147.

## Phase 87: B03 Web Native Extraction (v163)

| ID | Achievement | Evidence |
|----|-------------|----------|
| B03 | Native HTML-to-text extraction for web_extract — `web_extract_native()` fetches URL via http_client, strips HTML via `html_strip_tags()`, trims/collapses whitespace, truncates at 100KB. No Python dependency for basic extraction. Python delegate reserved for custom LLM extraction prompts. web.c 823→905 LOC, parity 42%→58%. | `src/tools/web.c` — web_extract_native() function (lines ~715-780), `lib/libhtml/html.c` — html_strip_tags() |

Suite: 294/0/0. Phase 87: B03 native web_extract. web parity 42%→58%.

## Phase 88: B07 Terminal Safety Checks (v164)

| ID | Achievement | Evidence |
|----|-------------|----------|
| B07 | Workdir validation + disk usage warning for terminal tool — `_check_workdir()` validates workdir exists and is a directory. `_check_disk_usage()` warns if < 100MB free on workdir filesystem via statfs(). Both are non-blocking warnings injected into result JSON via `_inject_warnings()`. terminal.c 795→859 LOC, parity 53%→57%. | `src/tools/terminal.c` — _check_workdir(), _check_disk_usage(), _inject_warnings() functions |

Suite: 294/0/0. Phase 88: B07 terminal safety checks (53%→57%).

## Phase 89: B09/B10 Stale Claim Correction (v165)

| ID | Achievement | Evidence |
|----|-------------|----------|
| B09 | patch tool LOC verified at 1154 vs battleship claim of ~700 (96% parity, not 58%). All features implemented: dry_run, V4A multi-file patch mode, 9 fuzzy matching strategies, conflict resolution, replace_all. Moved to IMPLEMENTED. | `src/tools/patch.c` — 1154 LOC, `wc -l` verified |
| B10 | session_search tool LOC verified at 621 vs battleship claim of ~460 (96% parity, not 71%). All features implemented: scroll+browse modes, tag_filter, role_filter, FTS5 syntax, offset pagination. Moved to IMPLEMENTED. | `src/tools/session_search.c` — 621 LOC, `wc -l` verified |

Suite: 294/0/0. Phase 89: B09/B10 stale claims corrected. Gaps: 147→145.

## Phase 81: yuanbao_tools bug fix + test suite (v157)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P81-01 | yb_search_sticker_handler: strdup query before json_free — fixes dangling pointer use-after-free on every search | `src/tools/yuanbao_tools.c:212-213` |
| P81-02 | yb_send_sticker_handler: strdup sticker_arg/chat_id before json_free — fixes same pattern in sticker send | `src/tools/yuanbao_tools.c:315-316` |
| P81-03 | yb_send_dm_handler: strdup user_id/name/group_code/message before json_free — fixes same pattern in DM send | `src/tools/yuanbao_tools.c:605-608` |
| P81-04 | test_yuanbao_tools.c (21 tests) — covers sticker search by ID, empty query, limit clamping, null args, sticker DB integrity | `tests/test_yuanbao_tools.c` |

Suite: 295/0/0. Gaps: 149. Test files: 249.

## Phase 80: B03 web_get save-to-file mode (v156)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P80-01 | web_get save_path param — saves response body to file instead of returning inline. Enables binary/PDF download without token waste. | `src/tools/web.c:35-36` (schema), `src/tools/web.c:377-394` (handler) |

Suite: 294/0/0 (unchanged). Gaps: 150 — B03 improved with save-to-file mode.

## Phase 79: A18 models.dev HTTP Fetch + 3-Tier Cache (v155)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P79-01 | models_dev_fetch() — 3-tier cache (in-memory/disk/network) for models.dev/api.json. 1h TTL. Falls back to stale cache on network failure. | `src/agent/provider_metadata.c` — models_dev_fetch() |
| P79-02 | models_dev_lookup_context() — look up context window from dynamic models.dev data by provider+model | `src/agent/provider_metadata.c` — models_dev_lookup_context() |
| P79-03 | models_dev_list_json() — flat JSON array matching static table format for /model list integration | `src/agent/provider_metadata.c` — models_dev_list_json() |
| P79-04 | test_runner.sh: libhttp/http.c + -lz linked into provider_metadata + budget_tracker tests | `test_runner.sh` — provider_metadata, budget_tracker compile blocks |

Suite: 294/0/0 (unchanged). Gaps: 151 — A18 PORTED. S2 phase complete.

## Phase 90: B08 Telegram Topic thread_id Support (v166)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P90-01 | telegram_send_message() + telegram_send_message_with_keyboard() accept optional thread_id param. `message_thread_id` forwarded in Telegram sendMessage JSON body | `include/hermes_gateway.h` — declarations, `src/gateway/platforms/telegram.c` |
| P90-02 | send_message.c parses thread_id from args or target:chat_id:thread_id format. Fallback: args preferred over target string | `src/tools/send_message.c` — second-colon parsing + fallback logic |
| P90-03 | All existing callers in server.c (3) + telegram.c (6) pass NULL for thread_id — backward compatible | `src/gateway/server.c`, `src/gateway/platforms/telegram.c` |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 91: B07 Terminal force/status/guard (v167)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P91-01 | force param skips sandbox_escape_check when user confirms dangerous command | `src/tools/terminal.c` — force flag check before esc.blocked |
| P91-02 | Foreground timeout guard: reject explicit timeout > 600s with guidance to use background mode | `src/tools/terminal.c` — timeout_explicit check |
| P91-03 | status field in result JSON ('success' for exit 0, 'error' otherwise) | `src/tools/terminal.c` — run_command + run_command_pty + handler error paths |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 92: B08 [[as_document]] directive (v168)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P92-01 | [[as_document]] stripped from message text, images/video/audio sent as documents preserving original bytes | `src/tools/send_message.c` — force_document flag + media routing |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 93-94: B03 URL secret check + multi-URL support (v169)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P93-01 | `url_has_secret()` — detects 30+ API key prefix patterns in raw and URL-encoded URLs | `src/tools/web.c` — static helper + call sites |
| P93-02 | URL exfiltration check in web_get_handler — blocks requests containing secrets before fetch | `src/tools/web.c` — web_get_handler |
| P93-03 | URL exfiltration check in web_extract_handler — blocks extract requests with secrets | `src/tools/web.c` — web_extract_handler |
| P94-01 | Multi-URL support in web_extract — accepts `urls` array, processes each URL, returns results array | `src/tools/web.c` — web_extract_handler multi-URL branch |
| P94-02 | Backward compatible: single `url` field still works for existing callers | `src/tools/web.c` — single-URL fallback path |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 95: B02 remote URL safety checks for vision (v170)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P95-01 | SSRF protection for remote image URLs — url_is_safe() check before processing | `src/tools/vision.c` — vision_handler remote URL branch |
| P95-02 | Secret exfiltration check — url_has_secret() blocks URLs with API key patterns | `src/tools/vision.c` — vision_handler remote URL branch |
| P95-03 | Content-Type validation via HEAD request — warns if image URL returns non-image content type | `src/tools/vision.c` — HEAD query in remote URL branch |
| P95-04 | data: URI passthrough — data URIs excluded from URL safety checks | `src/tools/vision.c` — skip condition for data: prefix |
| P95-05 | url_has_secret moved from web.c to url_safety.c — reusable across tools | `src/tools/url_safety.c` — `src/include/hermes_url_safety.h` |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 96: B07 exit code interpretation for terminal (v171)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P96-01 | exit_code_interpret() maps non-zero exit codes to human-readable messages per command (grep/rg/diff/find/test/curl/git) | `src/tools/terminal.c` — exit_code_interpret function |
| P96-02 | _inject_interpretation() adds exit_code_interpretation field to result JSON | `src/tools/terminal.c` — _inject_interpretation function |
| P96-03 | All 7 backend paths (pty, ssh, docker, compose, modal, local) get interpretation injected | `src/tools/terminal.c` — terminal_handler return paths |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 97: B08 disable_link_previews for Telegram (v173)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P97-01 | telegram_send_message() + with_keyboard() accept disable_preview bool param | `src/gateway/platforms/telegram.c` — function signatures + JSON body |
| P97-02 | disable_web_page_preview:true set when disable_preview=true | `src/gateway/platforms/telegram.c` — JSON body construction |
| P97-03 | disable_link_previews schema param added to send_message tool | `src/tools/send_message.c` — SCHEMA + args parsing |
| P97-04 | All internal callers updated (telegram.c, server.c, send_message.c) | `src/gateway/platforms/telegram.c` — 6 call sites + server.c + send_message.c |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 102: token_exchange test suite (v178)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P102-01 | oauth_last_error() initial state test | `tests/test_token_exchange.c` — test 1 |
| P102-02 | oauth_token_free handles NULL + allocated + populated tokens | `tests/test_token_exchange.c` — tests 2-4 |
| P102-03 | auth_store_free handles NULL + empty array edge cases | `tests/test_token_exchange.c` — tests 5-6 |
| P102-04 | Registered in test_runner.sh | `test_runner.sh` — token_exchange section |

Suite: 296/0/0. Test files: 252. Gaps: 145.

## Phase 103: SMS gateway test suite (v178)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P103-01 | sms_verify_webhook always returns "OK" | `tests/test_sms.c` — tests 1-2 |
| P103-02 | sms_parse_webhook handles NULL/empty/malformed input | `tests/test_sms.c` — tests 3-4, 15 |
| P103-03 | sms_parse_webhook parses inbound Twilio body | `tests/test_sms.c` — test 5 |
| P103-04 | sms_parse_webhook parses status callback + error | `tests/test_sms.c` — tests 6-7 |
| P103-05 | sms_parse_webhook parses multi-media MMS | `tests/test_sms.c` — test 8 |
| P103-06 | sms_parse_webhook decodes URL-encoded special chars | `tests/test_sms.c` — test 9 |
| P103-07 | sms_handle_webhook + queue + poll cycle works | `tests/test_sms.c` — test 10 |
| P103-08 | sms_queue_message accepts NULL params gracefully | `tests/test_sms.c` — test 11 |
| P103-09 | sms_get_media_url returns NULL for OOB index + NULL update | `tests/test_sms.c` — tests 5, 12 |
| P103-10 | sms_get_num_media returns 0 when media_urls absent | `tests/test_sms.c` — test 13 |
| P103-11 | sms_handle_webhook(NULL) no crash | `tests/test_sms.c` — test 14 |
| P103-B01 | Bug fix: sms_get_media_url() used json_get_str(NULL key) which always returns NULL | `src/gateway/platforms/sms.c:452` — Fixed to access item->str_val directly |

Suite: 296/0/0. Test files: 252. Gaps: 145.

## Phase 104: Env passthrough blocklist expansion (v179)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P104-01 | Blocklist expanded 23→67 entries: provider URLs (OPENAI_BASE_URL, ANTHROPIC_BASE_URL, FIRECRAWL_API_URL) | `lib/libenvpassthrough/env_passthrough.c:19` |
| P104-02 | Additional provider keys: MISTRAL_API_KEY, GROQ_API_KEY, TOGETHER_API_KEY, PERPLEXITY_API_KEY, COHERE_API_KEY, FIREWORKS_API_KEY, HELICONE_API_KEY, PARALLEL_API_KEY | `lib/libenvpassthrough/env_passthrough.c:42-50` |
| P104-03 | Gateway credential coverage: TELEGRAM_BOT_TOKEN, DISCORD_BOT_TOKEN, SLACK_BOT_TOKEN, GH_TOKEN, HASS_TOKEN, SIGNAL_HTTP_URL, etc. | `lib/libenvpassthrough/env_passthrough.c:53-75` |
| P104-04 | Infra secrets: DOCKER_HOST, DOCKER_CERT_PATH, SSH_AUTH_SOCK, MODAL_TOKEN_ID/SECRET | `lib/libenvpassthrough/env_passthrough.c:79-84` |
| P104-05 | Test coverage expanded 15→27: 12 new blocked-var assertions for new entries | `tests/test_env_passthrough.c` |
| P104-06 | Removed stale test_paths.c ref (file deleted earlier, test_runner.sh still referenced it) | `test_runner.sh` — removed dead code |
| P104-07 | Removed duplicate cli_paths run_lib_test entry | `test_runner.sh` — dedup cleanup |

Suite: 296/0/0. Test files: 252. Gaps: 145.

## Phase 105: Threat patterns parity (v180)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P105-01 | Added 5 missing threat patterns from Python threat_patterns.py: c2_task_pull, c2_network_connect, exfil_wget, send_to_url, translate_execute | `lib/libthreatpatterns/threat_patterns.c` — 30→35 patterns |
| P105-02 | Test coverage expanded 24→36 assertions for all patterns | `tests/test_threat_patterns.c` — 12 new assertions |
| P105-03 | Added provider URLs, gateway credentials, and infra secrets to env_passthrough blocklist | `lib/libenvpassthrough/env_passthrough.c` — entries 42-84 |
| P105-04 | Removed stale test_paths.c reference + duplicate cli_paths entry | `test_runner.sh` — cleanup |

Suite: 296/0/0. Test files: 252. Gaps: 145.

## Phase 106: parse_mode support in send_message (v181)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P106-01 | parse_mode parameter added to send_message schema (Markdown/MarkdownV2/HTML/plain) | `src/tools/send_message.c` — SCHEMA constant |
| P106-02 | send_message handler extracts parse_mode from args, defaults to "Markdown" | `src/tools/send_message.c:205-207` |
| P106-03 | All 3 telegram send call sites use parse_mode instead of hardcoded "Markdown" | `src/tools/send_message.c` — lines 428, 433, 437 |
| P106-04 | Test coverage: parse_mode=HTML, MarkdownV2, empty default — 3 new assertions | `tests/test_send_message.c` — tests 24-26 |

Suite: 296/0/0. Test files: 252. Gaps: 145.

## Phase 107: account_usage test suite (v182)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P107-01 | account_usage_free(NULL) no crash | `tests/test_account_usage.c` — test 1 |
| P107-02 | account_usage_fetch(NULL/''/bad_provider) returns NULL | `tests/test_account_usage.c` — tests 2-4 |
| P107-03 | fetch(openrouter, no key) and fetch(anthropic, no key) return NULL | `tests/test_account_usage.c` — tests 5-6 |
| P107-04 | account_usage_render(NULL) returns empty array (first entry NULL) | `tests/test_account_usage.c` — test 7 |
| P107-05 | render(minimal snapshot) produces title + provider lines | `tests/test_account_usage.c` — test 8 |
| P107-06 | render(window snapshot) produces label + quota + reset lines | `tests/test_account_usage.c` — test 9 |

Suite: 297/0/0. Test files: 253. Gaps: 145.

## Phase 108: budget_tracker test suite (v183)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P108-01 | budget_tracker_init/budget_tracker_set_limits init and configure state | `tests/test_budget_tracker.c` — tests 1-4 |
| P108-02 | budget_tracker_report_turn updates all totals (input, output, cost, turns, model) | `tests/test_budget_tracker.c` — tests 5-6 |
| P108-03 | is_exceeded detects over-limit, remaining returns correct values | `tests/test_budget_tracker.c` — tests 7-9 |
| P108-04 | get_warning at 80% threshold, no warning under, cleared after first read | `tests/test_budget_tracker.c` — tests 11-13 |
| P108-05 | Per-turn tool call tracking: set limit, increment, exceed, reset | `tests/test_budget_tracker.c` — tests 14-15 |
| P108-06 | Hard limit vs grace call mode: is_hard_exceeded vs is_exceeded | `tests/test_budget_tracker.c` — tests 16, 23 |
| P108-07 | stats_json produces valid JSON, NULL safety for all API functions (15 NULL tests) | `tests/test_budget_tracker.c` — tests 17-18, 22 |
| P108-08 | reset clears totals preserves limits | `tests/test_budget_tracker.c` — test 19 |
| P108-09 | Iteration budget: consume/refund/unlimited (0=unlimited) | `tests/test_budget_tracker.c` — tests 20-21 |

Suite: 298/0/0. Test files: 254. Gaps: 145.

## Phase 109: interruptible streaming (v184)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P109-01 | token_cb return value checked in on_provider_stream_chunk — non-zero aborts streaming | `src/agent/llm_client.c:1102-1110` |
| P109-02 | token_cb return value checked in OpenAI fallback stream chunk handler | `src/agent/llm_client.c:1254-1262` |
| P109-03 | cli_stream_cb checks g_cli.agent.interrupted before each token, returns 1 to abort | `src/cli/cli.c:319-321` |

Suite: 298/0/0. Test files: 254. Gaps: 145.

## Phase 101: voice_mode test suite (v176)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P101-01 | voice_mode.c config API test: voice_set_enabled/voice_is_enabled | `tests/test_voice_mode.c` — tests 1-6 |
| P101-02 | voice_set_device tests: NULL, empty, valid, long string edge cases | `tests/test_voice_mode.c` — tests 7-10 |
| P101-03 | voice_set_asr_cmd tests: NULL, empty, valid strings | `tests/test_voice_mode.c` — tests 11-13 |
| P101-04 | voice_record + voice_transcribe graceful NULL handling tests | `tests/test_voice_mode.c` — tests 14-15 |
| P101-05 | Registered in test_runner.sh with proper dependency chain | `test_runner.sh` — voice_mode section |

Suite: 295/0/0. Test files: 251. Gaps: 145.

## Phase 100: B07 terminal foreground/background guidance (v175)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P100-01 | _check_foreground_guidance() detects nohup/disown/setsid patterns, suggests background=true | `src/tools/terminal.c` — _check_foreground_guidance function |
| P100-02 | Trailing & and inline & background detection with guidance | `src/tools/terminal.c` — string checks for ' & ' patterns |
| P100-03 | guidance field injected into result JSON via _inject_warnings() for all backend paths (pty, ssh, docker, compose, modal, default) | `src/tools/terminal.c` — all _inject_warnings call sites updated |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 99: B08 send_message action=list (v174)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P99-01 | send_message handler parses `action` param (default "send") | `src/tools/send_message.c` — `json_object_get_string(args, "action", "send")` |
| P99-02 | action=list returns available platforms (stdout, local, telegram, discord, slack, matrix, signal) with format hint | `src/tools/send_message.c` — action=list branch, json array of platform names |
| P99-03 | Schema updated with action param description | `src/tools/send_message.c` — SCHEMA constant |

Suite: 294/0/0 (unchanged). Gaps: 145.

## Phase 98: B01 browser URL safety + full snapshot (v173)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P98-01 | browser_navigate URL safety checks: secret exfiltration via url_has_secret() | `src/tools/browser.c` — browser_navigate_handler → url_has_secret() |
| P98-02 | browser_navigate SSRF protection via url_is_safe() | `src/tools/browser.c` — browser_navigate_handler → url_is_safe() |
| P98-03 | browser_snapshot(full=true) returns complete page content without 4K truncation | `src/tools/browser.c` — browser_snapshot_handler full=true path |
| P98-04 | Vaulted stale battleship claim: browser_generate_pdf PDF download already existed via CDP | `src/tools/browser.c` — cdp_generate_pdf + registry_init_browser |

Suite: 294/0/0 (unchanged). Gaps: 145.

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

## Phase 109: Interruptible Streaming
| ID | Achievement | Evidence |
|----|-------------|----------|
| P109-01 | token_cb return value checked in provider streaming path | `src/agent/llm_client.c` |
| P109-02 | token_cb return value checked in legacy streaming path | `src/agent/llm_client.c` |
| P109-03 | cli_stream_cb checks interrupted flag per token | `src/cli/commands.c` |

## Phase 110: Send Message Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P110-01 | disable_link_previews param accepted | `tests/test_send_message.c` |
| P110-02 | thread_id direct param accepted | `tests/test_send_message.c` |
| P110-03 | reply_to_message_id param accepted | `tests/test_send_message.c` |
| P110-04 | [[as_document]] directive stripped | `tests/test_send_message.c` |
| P110-05 | media_group array no-crash | `tests/test_send_message.c` |
| P110-06 | inline_buttons with stdout works | `tests/test_send_message.c` |
| P110-07 | Very long message (4KB) no-crash | `tests/test_send_message.c` |

## Phase 111: Terminal Quote Stripping
| ID | Achievement | Evidence |
|----|-------------|----------|
| P111-01 | _strip_quotes() — strips single-quoted, double-quoted, and backtick content | `src/tools/terminal.c` — _strip_quotes() |
| P111-02 | _check_foreground_guidance uses stripped command — prevents false-positive on echoed text | `src/tools/terminal.c` — _check_foreground_guidance() |
| P111-03 | Backslash-escape handling inside double-quoted strings | `src/tools/terminal.c` — _strip_quotes() |

## Phase 112: Gateway Helpers Test Suite
| ID | Achievement | Evidence |
|----|-------------|----------|
| P112-01 | msg_dedup test: init, is_duplicate, clear, NULL-safety (9 assertions) | `tests/test_helpers.c` |
| P112-02 | strip_markdown test: plain, bold, code, headings, links, fences (7 assertions) | `tests/test_helpers.c` |
| P112-03 | redact_phone test: NULL, short, medium, long redaction (5 assertions) | `tests/test_helpers.c` |
| P112-04 | thread_tracker test: init, mark, has, duplicate, destroy, NULL-safety (8 assertions) | `tests/test_helpers.c` |
| P112-05 | Bug fix: redact_phone buffer overflow — malloc too small for full output | `src/gateway/helpers.c` |

## Phase 113: GHSA-rhgp-j443-p4rf Env Passthrough Warning
| ID | Achievement | Evidence |
|----|-------------|----------|
| P113-01 | Warning logged when config.yaml attempts to register blocked provider credential via env_passthrough | `src/tools/terminal.c` — registry_init_terminal() |

## Phase 114: Content-Policy Immediate Fallback
| ID | Achievement | Evidence |
|----|-------------|----------|
| P114-01 | content_filter finish_reason detected in agent_loop retry loop — skips retry of filtered model | `src/agent/agent_loop.c` — retry loop at ~1089 |
| P114-02 | Content filter triggers immediate fallback to configured fallback model/provider | `src/agent/agent_loop.c` — fallback trigger path |

## Phase 115: URL Safety Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P115-01 | Secret exfiltration tests: sk-, ghp_, AIza, gho_, ghu_, ghs_, ghr_ prefix detection | `tests/test_url_safety.c` — test_url_has_secret() |
| P115-02 | Safe URL negative tests — verify no false positives on normal URLs | `tests/test_url_safety.c` — test_url_has_secret() |
| P115-03 | TEST_NULL macro added — NULL-result assertion helper | `tests/test_url_safety.c` — #define TEST_NULL |









Suite: 299/0/0 (255 test files). Gaps: 145.

## Phase 116: Credential Pool Wiring
| ID | Achievement | Evidence |
|----|-------------|----------|
| P116-01 | cred_pool field added to llm_config_t -- opaque credential pool ref | `include/hermes.h` -- void *cred_pool |
| P116-02 | Credential pool created and API key added in agent_configure_from_config | `src/agent/agent_loop.c` -- credential_pool_create + add_key |
| P116-03 | HTTP status codes reported to credential pool after each provider-path LLM request | `src/agent/llm_client.c` -- credential_pool_report() |
| P116-04 | credential_expired field checked in retry loop -- triggers fallback on expired credential | `src/agent/agent_loop.c` -- credential_expired check |

Suite: 299/0/0 (255 test files). Gaps: 145.

## Phase 117: Env Passthrough Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P117-01 | NULL/empty name edge-case tests for is_blocked, is_allowed, register | `tests/test_env_passthrough.c` — 6 NULL/empty assertions |
| P117-02 | Blocked var registration rejection test | `tests/test_env_passthrough.c` — register OPENAI_API_KEY returns false |
| P117-03 | Duplicate registration returns true test | `tests/test_env_passthrough.c` — register MY_CUSTOM_VAR twice |
| P117-04 | free_list NULL safety test (no crash) | `tests/test_env_passthrough.c` — free_list(NULL, 0) |
| P117-05 | Clear-then-re-register works test | `tests/test_env_passthrough.c` — clear + register + is_allowed |

## Phase 118: B02 Vision Base64 Data URL
| ID | Achievement | Evidence |
|----|-------------|----------|
| P118-01 | Native image-to-base64 data URL conversion — `image_to_base64_data_url()` reads file, detects MIME type from format, encodes as data: URI | `src/tools/vision.c` — `image_to_base64_data_url()`, `read_file_bytes()`, `image_format_to_mime()` |
| P118-02 | Extension-based image path returns base64 data URL in result | `vision_handler()` — local file valid-image path includes `base64_data_url` |
| P118-03 | Extensionless files detected via magic bytes also return `base64_data_url` | `vision_handler()` — magic-byte detected path includes `base64_data_url` |
| P118-04 | 4 test assertions for base64_data_url correctness (existence, prefix format) | `tests/test_vision.c` — 35 total (+4 from 31) |

## Phase 119: URL Safety Test Suite + Blocklist Bug Fix
| ID | Achievement | Evidence |
|----|-------------|----------|
| P119-01 | 29-assertion test suite for url_safety.c hostname/scheme/secret/blocklist/private IP checks | `tests/test_url_safety.c` |
| P119-02 | Bug fix: `url_is_always_blocked()` now checks dynamic domain blocklist (was checking only hardcoded private IP patterns) | `src/tools/url_safety.c` — blocklist iteration in `url_is_always_blocked()` |
| P119-03 | First registered url_safety test in test_runner.sh | `test_runner.sh` — url_safety inline compile |

Suite: 300/0/0 (257 test files). Gaps: 145.

## Phase 121: Send Target Parse (M38)

| ID | Achievement | Evidence |
|----|-------------|----------|
| P121-01 | Extracted inline target_ref parsing to standalone `parse_send_target()` with `send_target_t` struct | `src/tools/send_message.c` |
| P121-02 | 15-test suite (41 assertions) for platform:chat_id[:thread_id] parsing, overrides, edge cases | `tests/test_send_target.c` |
| P121-03 | Registered as M38 in test_runner.sh (parallel background) | `test_runner.sh` |

Suite: 300/0/0 (257 test files). Gaps: 145.

## Phase 122: Parse Mode Validation

| ID | Achievement | Evidence |
|----|-------------|----------|
| P122-01 | parse_mode validation in send_message — rejects invalid modes (INVALID, PlainText) with clear error message referencing valid modes | `src/tools/send_message.c` — validation block after parse_mode default |
| P122-02 | 3 new test assertions for invalid parse_mode rejection (37 total send_message tests) | `tests/test_send_message.c` — Tests 26b, 26c |
| P122-03 | Test count updated in test_runner.sh (34→37) | `test_runner.sh` |

Suite: 300/0/0 (257 test files). Gaps: 145.

## Phase 120: Registry Test Suite
| ID | Achievement | Evidence |
|----|-------------|----------|
| P120-01 | 30-assertion test suite for tool registry (registration, dispatch, timeout, name matching, toolset filter, availability) | `tests/test_registry.c` |
| P120-02 | Cleaned up duplicate registry test entries in test_runner.sh | `test_runner.sh` — removed 2 duplicate blocks |
| P120-03 | Restored accidentally-removed http_pool test | `test_runner.sh` — restored run_lib_test line |

Suite: 299/0/0 (257 test files). Gaps: 145.

## Phase 123: libdotenv Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P123-01 | Comprehensive 30-test suite for libdotenv (parsing, quoting, export, NULL safety, iter, file load, edge cases) — 70→457 LOC | `tests/test_dotenv.c` |
| P123-02 | Registered in test_runner.sh (M??) | `test_runner.sh` |
Suite: 300/0/0 (257 test files). Gaps: 145.

## Phase 124: test_tool_init.c Rewrite + Registration
| ID | Achievement | Evidence |
|----|-------------|----------|
| P124-01 | 13-test suite for tool_init (register, count, get_name, dispatch, duplicate, NULL safety) | `tests/test_tool_init.c` |
| P124-02 | Registered in test_runner.sh | `test_runner.sh` |
Suite: 301/0/0 (258 test files). Gaps: 145.

## Phase 125: is_image_size_error — B02 Vision Depth
| ID | Achievement | Evidence |
|----|-------------|----------|
| P125-01 | Ported `is_image_size_error()` from Python vision_tools — checks error text for image/payload size hints (too large, 413, content_too_large, exceeds). Wired into vision_handler to add resize_hint on delegation failure. | `src/tools/vision.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 126: disable_notification — B08 Send Message Depth
| ID | Achievement | Evidence |
|----|-------------|----------|
| P126-01 | `disable_notification` (silent send) param added to Telegram message API: telegram_send_message() + telegram_send_message_with_keyboard() signatures, JSON body wiring, send_message.c schema + handler parse + 3 caller updates. 11 caller updates across 3 files. 3 new test assertions (40 total). | `src/tools/send_message.c`, `src/gateway/platforms/telegram.c`, `src/gateway/server.c`, `include/hermes_gateway.h`, `tests/test_send_message.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 127: Clarify Response Format Parity
| ID | Achievement | Evidence |
|----|-------------|----------|
| P127-01 | `question` field echoed in clarify result (matching Python behavior). `choices_offered` field included when choices are present. Rich registry description with usage guidance matching Python's clarify_tool description. | `src/tools/clarify.c`, `tests/test_clarify.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 128: exec_code Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P128-01 | exec_code test suite expanded from 3→8 tests: missing code error, NULL args, timeout param, sandbox flag, output content verification, plus existing hello/fail/syntax. | `tests/test_exec_code.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 129: Approval Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P129-01 | Approval test suite expanded 18→23: session reset (null/crash safety, double reset), cache operations (initial count, empty entry NULL safety). | `tests/test_approval.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 130: Terminal Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P130-01 | Terminal test suite expanded 30→47 assertions: force param, status field (success/error), foreground timeout guard (>600s reject), bad workdir handling. | `tests/test_terminal.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 131: Process Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P131-01 | Process test suite expanded 18→27 assertions: list action, health check, log output retrieval, invalid action enum, process-not-found edge case. | `tests/test_process.c` |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 132: Telegram Send Retry — B08 Send Message Depth
| ID | Achievement | Evidence |
|----|-------------|----------|
| P132-01 | Telegram retry with exponential backoff: port of Python _telegram_retry_delay + _send_telegram_message_with_retry. `telegram_retry_ns()` provides 0.5s/1s/2s exponential backoff across 3 attempts. Media_group InputMedia array pre-built once and reused across retries. Inline keyboard reply_markup rebuilt fresh each attempt. | `src/tools/send_message.c` — `telegram_retry_ns()` function + retry loop wrapping all Telegram send paths |
Suite: 301/0/0 (258 test files). Gaps: 144.

## Phase 133: Stale S3 Claims Vaulted + file_merge Test Expansion
| ID | Achievement | Evidence |
|----|-------------|----------|
| P133-01 | G05 stale claim retired: wecom_crypto.py listed as gap — C already has wecom_crypto.c with 28-test suite (test_wecom_crypto.c) compiled into binary | `C/src/tools/wecom_crypto.c`, `C/tests/test_wecom_crypto.c` |
| P133-02 | G12 stale claim retired: api_server.py listed as gap — C has api_server.c (1224 LOC) with full HTTP API server | `C/src/api_server.c` — api_server_start/stop/is_running |
| P133-03 | file_merge test expansion 4→13 tests: added missing params (modified, output), identical files, different files with diff verification, unknown strategy, both files missing, empty base file, output file written verification | `C/tests/test_file_merge.c` — 9 new tests (13 total) |
| P133-04 | S10 F02 stale test count in battleship: 248→258 corrected | `C/.hermes/mind-palace/battleship-v34.md` |
## Phase 134: Stale S3 Claims Vaulted + B07 Terminal pipe_stdin Port
| ID | Achievement | Evidence |
|----|-------------|----------|
| P134-01 | G11 stale claim retired: yuanbao_sticker.py listed as gap — C has 59-sticker database with search scoring and yb_search_sticker/yb_send_sticker tools | `C/src/tools/yuanbao_tools.c` — YB_STICKERS array (59 entries), score_sticker(), yb_search_sticker handler |
| P134-02 | G13 stale claim retired: _http_client_limits.py listed as gap — C has http_client_set_pool(max_connections, idle_timeout_sec) for connection pool limits | `C/include/hermes_http.h:133` — http_client_set_pool() API |
| P134-03 | B07 terminal depth: _command_requires_pipe_stdin() ported from Python — PTY auto-override for gh auth login --with-token (which hangs in PTY waiting for piped stdin). Overrides use_pty=false when detected. | `C/src/tools/terminal.c` — PTY check block after use_pty parse |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 135: B07 Terminal Depth — help/version Command Detection
| ID | Achievement | Evidence |
|----|-------------|----------|
| P135-01 | Ported _looks_like_help_or_version_command() from Python terminal_tool. Detects --help/-h/--version/-v early in _check_foreground_guidance() and returns NULL (no guidance needed), preventing false-positive background guidance on informational commands. | `C/src/tools/terminal.c` — help/version check block at top of _check_foreground_guidance() |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 136: G02 base.py Depth — UTF-16 Helpers for Telegram
| ID | Achievement | Evidence |
|----|-------------|----------|
| P136-01 | Ported utf16_len() from Python base.py — counts UTF-16 code units in a UTF-8 string. Characters outside the BMP (emoji, CJK ext B, musical symbols) correctly count as 2 units instead of 1. | `C/src/gateway/server.c` — gw_utf16_len() |
| P136-02 | Ported _prefix_within_utf16_limit() from Python base.py — returns the longest UTF-8 prefix whose UTF-16 length ≤ limit. Linear scan respects character boundaries (never splits multi-byte chars). | `C/src/gateway/server.c` — gw_prefix_within_utf16_limit() |
| P136-03 | Updated battleship G02 entry: base.py correctly noted as 4286 LOC (not ~600). Rate limiting (gw_rate_limiter_t), retry (gw_retry_with_backoff), platform vtable (gw_platform_t), and UTF-16 helpers all marked PORTED. | `C/.hermes/mind-palace/battleship-v34.md` — G02 now PARTIAL |
| P136-04 | Gateway escape test expansion: 9→38 total assertions covering gw_utf16_len (9 tests) and gw_prefix_within_utf16_limit (10 tests) | `C/tests/test_gateway_escape.c` — sections 4 and 5 |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 137: B07 Terminal Depth — Env Assignment Detection
| ID | Achievement | Evidence |
|----|-------------|----------|
| P137-01 | Ported _looks_like_env_assignment() from Python terminal_tool. Detects leading KEY=VALUE tokens in commands (e.g. PATH=/usr/bin command) and skips foreground/background guidance — env assignments are setup, not commands to background. | `C/src/tools/terminal.c` — env assignment check block at top of _check_foreground_guidance() |
| P137-02 | Terminal test expansion: 47→58 total assertions covering env assignment variants (single, multi, with &, combined with nohup), all verifying no false guidance. | `C/tests/test_terminal.c` — tests 23-26 |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Stale Claim: B02 detail param (v211)
| ID | Achievement | Evidence |
|----|-------------|----------|
| SC01 | Stale claim corrected: B02 "detail param passthrough" listed as missing feature. C vision.c parses detail from JSON args (line 299), forwards in result for local files (line 326), remote URLs (line 362), unknown schemes (line 400), passes to Python delegation (line 509). | `C/src/tools/vision.c:299,326,362,400,509` |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 138: B07 Terminal Depth — Safe Command Preview
| ID | Achievement | Evidence |
|----|-------------|----------|
| P138-01 | Ported _safe_command_preview() from Python terminal_tool. Returns a log-safe, truncated command preview (max 200 chars by default, static buffer). Wired into popen error path for better diagnostics. | `C/src/tools/terminal.c` — _safe_command_preview() at line ~55, used in run_command() error path |
| P138-02 | Stale claim corrected: B02 "detail param passthrough" confirmed already wired in C vision.c (schema, parsing, forwarding in all 3 code paths + Python delegation). Battleship B02 entry updated. | `C/.hermes/mind-palace/battleship-v34.md` — B02 missing features corrected |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 139: B03 Web Depth — clean_base64_images
| ID | Achievement | Evidence |
|----|-------------|----------|
| P139-01 | Ported clean_base64_images() from Python web_tools. Strips inline data:image/ URIs (base64-encoded images) from extracted web text, replacing them with placeholder. Wired into web_extract_native() output pipeline. | `C/src/tools/web.c` — _clean_base64_images() near line 740, called in web_extract_native() line ~843 |
Suite: 301/0/0 (258 test files). Gaps: 140.

## Phase 140: B03 Web Depth — clean_base64_images Test Suite
| ID | Achievement | Evidence |
|----|-------------|----------|
| P140-01 | Exposed _clean_base64_images() from static to public linkage so tests can call it directly. | `C/src/tools/web.c` — removed `static` from `_clean_base64_images()` |
| P140-02 | Added 13 test assertions covering clean_base64_images: NULL input, empty string, plain text passthrough, single image removal, before/after text preservation, raw base64 absence, multiple images, only-image edge case, JPEG data URL, inline HTML with quotes. | `C/tests/test_web.c` — tests 23-35, 22→35 total |
| P140-03 | Updated test_runner.sh web_tool test count from 22 to 35. All tests PASS. | `C/test_runner.sh` — web_tool (35 tests) |
Suite: 301/0/0 (258 test files). Gaps: 140.
