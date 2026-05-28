     1|# Vault of Achievements — Slermes C Translation
     2|
     3|> Archive of completed milestones, resolved gaps, and retired stale claims.
     4|> Every entry was verified against running source code at time of retirement.
     5|
     6|---
     7|
     8|## Phase 1: Foundation & Core Infrastructure
     9|
    10|| ID | Achievement | Evidence |
    11||----|-------------|----------|
    12|| F01 | C build system (Makefile) — compiles 173 .c files, 73 headers, 65 libs | `make -j$(nproc)` clean |
    13|| F02 | Binary 30M, 282/0/0 test suite | `basher test_runner.sh` |
    14|| F03 | 99 registered tools | `grep registry_register src/tools/*.c` |
    15|| F04 | 98 CLI commands | `grep '^    {' src/cli/commands.c` |
    16|| F05 | 19 gateway platforms | `ls src/gateway/platforms/*.c` |
    17|| F06 | 10 provider types | `ls src/agent/provider_*.c` |
    18|| F07 | 65 library modules | `ls -d lib/lib*` |
    19|
    20|## Phase 2: Agent Core
    21|
    22|| ID | Achievement | Evidence |
    23||----|-------------|----------|
    24|| A01 | Agent conversation loop (agent_loop.c, 1532 LOC) | Compiles, tested via suite |
    25|| A02 | LLM API client (llm_client.c, 1569 LOC) | All 10 providers |
    26|| A03 | Provider infrastructure: OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, LMStudio | `src/agent/provider_*.c` |
    27|| A04 | Provider metadata + fallback routing + credential pool | Integration tested |
    28|| A05 | Budget tracker | `test_budget_tracker.c` (58 tests) |
    29|| A06 | System prompt builder (system_prompt.c) | Integration tested |
    30|| A07 | Context engine (context_engine.c) | Integration tested |
    31|| A08 | Prompt caching (prompt_caching.c) | `test_prompt_caching.c` (23 tests) |
    32|| A09 | Context references (context_references.c) | `test_context_references.c` (32 tests) |
    33|| A10 | Gemni/Moonshot schema adapters | Integration tested |
    34|| A11 | Redact, sanitize, vault, think_scrubber | All tested |
    35|| A12 | Checkpoint manager | Integration tested |
    36|| A13 | Markov tables renderer | `test_markdown.c` (15 tests) |
    37|| A14 | Portal tags, tool guardrails, skill preprocessing | Integration tested |
    38|| A15 | Onboarding, i18n, subdir hints | All compiled |
    39|| A16 | Tool guardrails (tirith) | `test_tirith.c` (79 tests) |
    40|
    41|## Phase 3: CLI & Commands
    42|
    43|| ID | Achievement | Evidence |
    44||----|-------------|----------|
    45|| C01 | CLI entry point — `--help`, `--version`, `--json`, pipe mode | Verified live |
    46|| C02 | 98 slash commands in commands.c (4635 LOC) | `grep '^    {' commands.c` |
    47|| C03 | Config system (config.c, 3869 LOC) | `test_config.c` (103 tests) |
    48|| C04 | Display system (display_core.c + display.c) | `test_display.c` (28+ tests) |
    49|| C05 | TUI fullscreen mode (tui_fullscreen.c, 3349 LOC) | Compiled |
    50|| C06 | Skin engine | Compiled |
    51|| C07 | Paths module | `test_paths.c` (11 tests) |
    52|
    53|## Phase 4: Tools
    54|
    55|| ID | Achievement | Evidence |
    56||----|-------------|----------|
    57|| T01 | file operations: read/write/patch/search_files + file_diff, hash, hex, perms, syntax, merge, watch, batch | `test_file.c` (61 tests), `test_file_watch.c` (12 tests) |
    58|| T02 | Web tools: web_get, web_search (5 backends), web_extract | `test_web.c` (22 tests) |
    59|| T03 | Browser automation (browser.c, 1586 LOC) — navigate, click, type, scroll, snapshot, CDP, console, dialog, supervisor, vision | `test_browser.c` (14 tests) |
    60|| T04 | Terminal tool (terminal.c) with local + Docker + SSH + Modal backends | `test_terminal.c` |
    61|| T05 | Process management (process.c, 559 LOC) — start/poll/wait/kill/log/signal/write/submit/close, checkpoint save/load | `test_process.c` (13 tests) |
    62|| T06 | Memory tool (memory.c, 122 functions) — file-based + plugin backends | `test_memory.c` (17 tests) |
    63|| T07 | Todo tool — write/merge, status, summary | Parity with Python |
    64|| T08 | Clarify tool — ask user questions | Parity with Python |
    65|| T09 | Send message tool — multi-platform dispatch | Embedded in gateway |
    66|| T10 | Delegate task — subagent spawning | `test_delegate.c` |
    67|| T11 | Skills system — 16 skill tools (list/view/manage/bundle/cache/curator/deps/hub/provenance/scan/search/sync/usage/validate) | `test_skills.c`, `test_skill_mgmt.c` |
    68|| T12 | Session search + CRUD | `test_session_search.c` (13 tests) |
    69|| T13 | Vision analysis | `test_vision.c` (21 tests) |
    70|| T14 | Image generation | `test_image_gen.c` (9 tests) |
    71|| T15 | Video generation + analysis | `test_video_gen.c` (6 tests) |
    72|| T16 | Text-to-speech | `test_tts.c` (21 tests) |
    73|| T17 | Transcribe audio | `test_transcribe.c` (9 tests) |
    74|| T18 | X (Twitter) search | `test_x_search.c` (14 tests) |
    75|| T19 | Home Assistant — 5 tools | `test_homeassistant.c` (26 tests) |
    76|| T20 | Discord — 2 tools | `test_discord.c` (31 tests) |
    77|| T21 | Kanban — 9 tools | `test_kanban.c` (39 tests) |
    78|| T22 | Computer use — macOS + X11 + Wayland + noop backends | `test_computer_use.c` |
    79|| T23 | Feishu tools — doc_read, drive (add/list/reply comment) | `test_feishu_tools.c` (42 tests) |
    80|| T24 | Voice mode — listen/speak | `test_voice_mode.c` |
    81|| T25 | Cronjob management | `test_cronjob.c` |
    82|| T26 | Approval system | `test_edit_approval.c` (44 tests) |
    83|| T27 | MCP tool integration | `test_mcp.c`, `test_mcp_stream.c`, `test_mcp_oauth.c` |
    84|| T28 | MCP OAuth flow | `test_mcp_oauth.c` (13 tests) |
    85|| T29 | Patch tool — 9 fuzzy matching strategies | `test_patch.c` |
    86|| T30 | Execute code tool | `test_exec_code.c` |
    87|| T31 | Tool output limits | `test_tool_output.c` (21 tests) |
    88|| T32 | Tool result storage | `test_result_storage.c` |
    89|| T33 | Website policy enforcement | `test_website_policy.c` (11 tests) |
    90|| T34 | OSV vulnerability check | `test_osv.c` (11 tests) |
    91|| T35 | Credential management | `test_credential.c`, `test_credential_pool.c` |
    92|| T36 | Account usage tracking | `test_account_usage.c` (11 tests) |
    93|| T37 | Binary extensions | `test_binary.c` (14 tests) |
    94|| T38 | WeCom crypto (WXBizMsgCrypt) | `test_wecom_crypto.c` (28 tests) |
    95|| T39 | Message sanitization (repair tool call arguments) | `test_sanitize.c` (20 tests) |
    96|
    97|## Phase 5: Gateway Platforms
    98|
    99|| ID | Achievement | Evidence |
   100||----|-------------|----------|
   101|| G01 | Gateway server (server.c, 2143 LOC) — webhook queue, async prompt response, connection pool, keepalive | `test_managed_gateway.c` |
   102|| G02 | Telegram platform | `test_telegram.c` |
   103|| G03 | Discord platform | `test_discord_interactions.c` |
   104|| G04 | Webhook platform | Integration tested |
   105|| G05 | Slack platform | Compiled |
   106|| G06 | Matrix platform | Compiled |
   107|| G07 | Mattermost platform | Compiled |
   108|| G08 | WhatsApp platform | Compiled |
   109|| G09 | Email platform (IMAP/SMTP) | Compiled |
   110|| G10 | Signal platform | Compiled |
   111|| G11 | Home Assistant platform | Compiled |
   112|| G12 | SMS platform | Compiled |
   113|| G13 | Feishu platform | Compiled |
   114|| G14 | WeCom platform | Compiled |
   115|| G15 | DingTalk platform | Compiled |
   116|| G16 | QQ Bot platform | Compiled |
   117|| G17 | BlueBubbles platform | Compiled |
   118|| G18 | MS Graph webhook platform | Compiled |
   119|| G19 | Weixin platform | Compiled |
   120|| G20 | YuanBao platform | Compiled |
   121|| G21 | Gateway reactions (send_reaction vtable) | `test_gateway_reactions.c` |
   122|
   123|## Phase 6: Libraries
   124|
   125|| ID | Achievement | Evidence |
   126||----|-------------|----------|
   127|| L01 | libhttp — HTTP client with streaming, SSL, retry, cookies, auth | 1655 LOC, 101 functions |
   128|| L02 | libjson — JSON parser/serializer with surrogate pair support | 693 LOC, 40 functions |
   129|| L03 | libmcp — MCP protocol client with SSE transport, OAuth | 2093 LOC, 105 functions |
   130|| L04 | libdb — SQLite session storage | Bundled sqlite3 |
   131|| L05 | libcrypto — AES, SHA1, SHA256, HMAC, base64url, PKCS7 | 511 LOC, 41 functions |
   132|| L06 | libplugin — Plugin loading and management | 881 LOC, 65 functions |
   133|| L07 | libtui — TUI framework (ncurses-based) | 487 LOC, 32 functions |
   134|| L08 | libwebsocket — WebSocket client | 372 LOC, 19 functions |
   135|| L09 | libyaml — YAML parser | 563 LOC, 39 functions |
   136|| L10 | libglob — Glob pattern matching | 163 LOC, 7 functions |
   137|| L11 | libregex — Regex wrapper | 119 LOC, 14 functions |
   138|| L12 | libcsv — CSV parser | 398 LOC, 24 functions |
   139|| L13 | libdatetime — Date/time utilities | 373 LOC, 35 functions |
   140|| L14 | libhash — SHA-256, MD5, HMAC | 177 LOC, 14 functions |
   141|| L15 | libbase64 — Base64 encode/decode | 199 LOC, 19 functions |
   142|| L16 | libansi — ANSI escape handling | 358 LOC, 27 functions |
   143|| L17 | libhtml — HTML parsing utilities | 166 LOC, 13 functions |
   144|| L18 | libdifflib — Diff/patch utilities | 242 LOC, 31 functions |
   145|| L19 | libpath — Path manipulation | 417 LOC, 45 functions |
   146|| L20 | libcron — Cron expression parsing | 340 LOC, 17 functions |
   147|| L21 | libproc — Process management helpers | 205 LOC, 10 functions |
   148|| L22 | libuuid — UUID generation | 168 LOC, 15 functions |
   149|| L23 | libdotenv — .env file parser | 252 LOC, 15 functions |
   150|| L24 | libdebug — Debug helper utilities | 211 LOC, 9 functions |
   151|| L25 | libosv — OSV vulnerability database client | 283 LOC, 15 functions |
   152|| L26 | libwebsite — Website policy enforcement | 245 LOC, 21 functions |
   153|| L27 | libenvpassthrough — Env var passthrough config | 154 LOC, 8 functions |
   154|| L28 | libxai_http — xAI HTTP client | 51 LOC, 3 functions |
   155|| L29 | libcredential — Credential management | 537 LOC, 26 functions |
   156|| L30 | libschemasanitizer — JSON schema sanitization | 628 LOC, 47 functions |
   157|| L31 | libfuzzymatch — Fuzzy string matching | 738 LOC, 79 functions |
   158|| L32 | libinterrupt — Cross-platform interrupt handling | 71 LOC, 7 functions |
   159|| L33 | libfilestate — File state tracking | 411 LOC, 32 functions |
   160|| L34 | libtooldispatch — Tool dispatch helpers | 305 LOC, 15 functions |
   161|| L35 | librateguard — Rate limiting guards | 204 LOC, 16 functions |
   162|| L36 | libratelimit — Rate limit tracking | 386 LOC, 31 functions |
   163|| L37 | libskillutils — Skill utility functions | 652 LOC, 35 functions |
   164|| L38 | liberrorclassifier — Error classification | 812 LOC, 40 functions |
   165|| L39 | libfile_sync — File synchronization | 243 LOC, 12 functions |
   166|| L40 | libbudgetconfig — Budget configuration | 90 LOC, 4 functions |
   167|| L41 | libthreatpatterns — Security threat patterns | 301 LOC, 15 functions |
   168|| L42 | libcredentialfiles — Credential file management | 340 LOC, 20 functions |
   169|| L43 | libskillaudit — Skill security audit | 383 LOC, 19 functions |
   170|| L44 | libslashconfirm — Slash command confirmation | 210 LOC, 17 functions |
   171|| L45 | libmsgraph — Microsoft Graph API client | 517 LOC, 26 functions |
   172|| L46 | libsignal — Signal handling | 66 LOC, 9 functions |
   173|| L47 | libbinary — Binary file utilities | 96 LOC, 2 functions |
   174|| L48 | libbrowser — Browser Camofox state | 226 LOC, 13 functions |
   175|| L49 | libtemplate — Template engine | 554 LOC, 33 functions |
   176|| L50 | libtoml — TOML parser | 514 LOC, 21 functions |
   177|| L51 | libjson5 — JSON5 parser | 481 LOC, 22 functions |
   178|| L52 | libmcp_oauth — MCP OAuth flow | 1262 LOC, 98 functions |
   179|| L53 | libfal_common — FAL API common utilities | 95 LOC, 5 functions |
   180|| L54 | libtooloutput — Tool output limits | 56 LOC, 6 functions |
   181|| L55 | libmangateway — Managed gateway support | 280 LOC, 14 functions |
   182|| L56 | libcreditfiles — Credential files | Included |
   183|| L57 | libbrowser — browser state management | Included |
   184|| L58 | libtranscribe — Audio transcription | 611 LOC, 29 functions |
   185|| L59 | libtoml — TOML serialization | Included |
   186|| L60 | libbinary — Binary data handling | Included |
   187|| L61 | liblineedit — Line editing | 594 LOC, 31 functions |
   188|| L62 | libskillusage — Skill usage tracking + provenance | 622 LOC, 37 functions |
   189|| L63 | libskillsync — Skill sync + diff | 707 LOC, 41 functions |
   190|| L64 | libwebsearchregistry — Web search registry | Separate compilation |
   191|| L65 | libncurses — ncurses headers (bundled, WSL compat) | 10857 LOC headers only |
   192|
   193|## Phase 7: Stale Claims Retired
   194|
   195|Claims from past battleship versions verified as already implemented.
   196|
   197|| ID | Old Claim | Reality | Evidence |
   198||----|-----------|---------|----------|
   199|| ~80% of 1A depth gaps | Various tool feature gaps | Already implemented in C | Verified via grep 2026-05-27 |
   200|| S07 | shutdown = NULL (S07) | Resolved via gw_platform_shutdown_all() | server.c |
   201|| S01/S02 | Browser CDP stubs | Real WebSocket/JSON-RPC | browser.c |
   202|| D09 | CUA backend missing | Implemented at computer_use.c | ~400 LOC |
   203|| D12 | CDP backend stub | Real implementation | browser.c |
   204|| L14-L29 | Various library depth claims | Already implemented | Verified via grep |
   205|
   206|## Phase 8: Gateway Shutdown Callback
   207|
   208|| ID | Achievement | Evidence |
   209||----|-------------|----------|
   210|| S02 | server.c:2066 — replaced `plat.shutdown = NULL` with `poll_platform_shutdown()` real callback | `src/gateway/server.c:1149` — `gw_platform_shutdown_all()` now calls logging callback per platform |
   211|| L02 | libmcp SSE transport: transport_send captures POST response body in recv_buf, transport_read_response parses and matches by request_id — SSE request-response now functional | `lib/libmcp/mcp.c` — recv_buf/recv_len + transport_send/read_response SSE paths |
   212|| S04 | commands.c:2595 — `reload plugins` calls hermes_plugin_shutdown + hermes_plugin_init (was stub) | `src/cli/commands.c` — hot-reload path |
   213|| S08 | homeassistant.c:524 — changed `Deprecated: use 'data' instead` to `Legacy alias for 'data' parameter` | `src/tools/homeassistant.c` — JSON schema string fix |
   214|| S03 | commands.c:2704-2747 — `/restart` now saves session, closes DB, execv with full path, strerror on failure, removed ARG_MAX stack waste | `src/cli/commands.c` — session save + execv + errno handling |
   215|
   216|## Phase 9: Stale Claims Retired (Triple DA v19 → v20)
   217|
   218|Claims from battleship-v19 verified against running source — all already implemented.
   219|
   220|| ID | Old Claim | Reality | Evidence |
   221||----|-----------|---------|----------|
   222|| F01 | `--bogus` sends to LLM (P0) | Error: "unknown flag '--bogus-flag'" | `./slermes --bogus-flag` returns proper error |
   223|| F02 | Multi-line pipe mode broken (P0) | `/help /tools /exit` processes each command | `printf '/help\n/tools\n' | ./slermes` works |
   224|| F03 | `--session` without arg runs session_crud (P0) | Error: "--session requires a session ID" | `./slermes --session` returns proper error |
   225|| F04 | Tools display shows 83 (stale, P1) | Shows "Registered tools (99)" | live binary `/tools` command |
   226|| M01 | discord tool missing (P2) | `discord` tool registered with 12 actions | `src/tools/discord.c:482` — registry_register("discord") |
   227|| M02 | discord_admin missing (P2) | `discord_admin` tool registered | `src/tools/discord.c:510` — registry_register("discord_admin") |
   228|| L02 | libmcp SSE streaming missing | SSE transport state, event buffer, POST to SSE endpoint | `lib/libmcp/mcp.c:45-56` — SSE transport fields |
   229|| I01 | No Dockerfile | Dockerfile exists (1177 bytes) | `C/Dockerfile` — build works with `docker build` |
   230|
   231|Note: X01-X05 (test coverage gaps) were listed as "0 test files" — each file exists with substantive content (vision: 229L, image_gen: 76L, video_gen: 61L, transcribe: 95L, session_search: 285L). Edge case expansion P3 remains.
   232|
## Phase 10: Gateway Vtable Wiring

| ID | Achievement | Evidence |
|----|-------------|----------|
| G02 | Telegram send_reaction wired to gw_platform_t vtable — `gw_platform_send_reaction(platform, chat_id, message_id, emoji)` now dispatches to `telegram_set_message_reaction()` via `telegram_vtable_send_reaction` wrapper in server.c | `src/gateway/server.c:1148-1154` — wrapper; `src/gateway/server.c:2086-2092` — wiring in registration loop |
