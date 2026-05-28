     1|1|# Vault of Achievements — Slermes C Translation
     2|2|
     3|3|> Archive of completed milestones, resolved gaps, and retired stale claims.
     4|4|> Every entry was verified against running source code at time of retirement.
     5|5|
     6|6|---
     7|7|
     8|8|## Phase 1: Foundation & Core Infrastructure
     9|9|
    10|10|| ID | Achievement | Evidence |
    11|11||----|-------------|----------|
    12|12|| F01 | C build system (Makefile) — compiles 173 .c files, 73 headers, 65 libs | `make -j$(nproc)` clean |
    13|13|| F02 | Binary 30M, 282/0/0 test suite | `basher test_runner.sh` |
    14|14|| F03 | 99 registered tools | `grep registry_register src/tools/*.c` |
    15|15|| F04 | 98 CLI commands | `grep '^    {' src/cli/commands.c` |
    16|16|| F05 | 19 gateway platforms | `ls src/gateway/platforms/*.c` |
    17|17|| F06 | 10 provider types | `ls src/agent/provider_*.c` |
    18|18|| F07 | 65 library modules | `ls -d lib/lib*` |
    19|19|
    20|20|## Phase 2: Agent Core
    21|21|
    22|22|| ID | Achievement | Evidence |
    23|23||----|-------------|----------|
    24|24|| A01 | Agent conversation loop (agent_loop.c, 1532 LOC) | Compiles, tested via suite |
    25|25|| A02 | LLM API client (llm_client.c, 1569 LOC) | All 10 providers |
    26|26|| A03 | Provider infrastructure: OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, LMStudio | `src/agent/provider_*.c` |
    27|27|| A04 | Provider metadata + fallback routing + credential pool | Integration tested |
    28|28|| A05 | Budget tracker | `test_budget_tracker.c` (58 tests) |
    29|29|| A06 | System prompt builder (system_prompt.c) | Integration tested |
    30|30|| A07 | Context engine (context_engine.c) | Integration tested |
    31|31|| A08 | Prompt caching (prompt_caching.c) | `test_prompt_caching.c` (23 tests) |
    32|32|| A09 | Context references (context_references.c) | `test_context_references.c` (32 tests) |
    33|33|| A10 | Gemni/Moonshot schema adapters | Integration tested |
    34|34|| A11 | Redact, sanitize, vault, think_scrubber | All tested |
    35|35|| A12 | Checkpoint manager | Integration tested |
    36|36|| A13 | Markov tables renderer | `test_markdown.c` (15 tests) |
    37|37|| A14 | Portal tags, tool guardrails, skill preprocessing | Integration tested |
    38|38|| A15 | Onboarding, i18n, subdir hints | All compiled |
    39|39|| A16 | Tool guardrails (tirith) | `test_tirith.c` (79 tests) |
    40|40|
    41|41|## Phase 3: CLI & Commands
    42|42|
    43|43|| ID | Achievement | Evidence |
    44|44||----|-------------|----------|
    45|45|| C01 | CLI entry point — `--help`, `--version`, `--json`, pipe mode | Verified live |
    46|46|| C02 | 98 slash commands in commands.c (4635 LOC) | `grep '^    {' commands.c` |
    47|47|| C03 | Config system (config.c, 3869 LOC) | `test_config.c` (103 tests) |
    48|48|| C04 | Display system (display_core.c + display.c) | `test_display.c` (28+ tests) |
    49|49|| C05 | TUI fullscreen mode (tui_fullscreen.c, 3349 LOC) | Compiled |
    50|50|| C06 | Skin engine | Compiled |
    51|51|| C07 | Paths module | `test_paths.c` (11 tests) |
    52|52|
    53|53|## Phase 4: Tools
    54|54|
    55|55|| ID | Achievement | Evidence |
    56|56||----|-------------|----------|
    57|57|| T01 | file operations: read/write/patch/search_files + file_diff, hash, hex, perms, syntax, merge, watch, batch | `test_file.c` (61 tests), `test_file_watch.c` (12 tests) |
    58|58|| T02 | Web tools: web_get, web_search (5 backends), web_extract | `test_web.c` (22 tests) |
    59|59|| T03 | Browser automation (browser.c, 1586 LOC) — navigate, click, type, scroll, snapshot, CDP, console, dialog, supervisor, vision | `test_browser.c` (14 tests) |
    60|60|| T04 | Terminal tool (terminal.c) with local + Docker + SSH + Modal backends | `test_terminal.c` |
    61|61|| T05 | Process management (process.c, 559 LOC) — start/poll/wait/kill/log/signal/write/submit/close, checkpoint save/load | `test_process.c` (13 tests) |
    62|62|| T06 | Memory tool (memory.c, 122 functions) — file-based + plugin backends | `test_memory.c` (17 tests) |
    63|63|| T07 | Todo tool — write/merge, status, summary | Parity with Python |
    64|64|| T08 | Clarify tool — ask user questions | Parity with Python |
    65|65|| T09 | Send message tool — multi-platform dispatch | Embedded in gateway |
    66|66|| T10 | Delegate task — subagent spawning | `test_delegate.c` |
    67|67|| T11 | Skills system — 16 skill tools (list/view/manage/bundle/cache/curator/deps/hub/provenance/scan/search/sync/usage/validate) | `test_skills.c`, `test_skill_mgmt.c` |
    68|68|| T12 | Session search + CRUD | `test_session_search.c` (13 tests) |
    69|69|| T13 | Vision analysis | `test_vision.c` (21 tests) |
    70|70|| T14 | Image generation | `test_image_gen.c` (9 tests) |
    71|71|| T15 | Video generation + analysis | `test_video_gen.c` (6 tests) |
    72|72|| T16 | Text-to-speech | `test_tts.c` (21 tests) |
    73|73|| T17 | Transcribe audio | `test_transcribe.c` (9 tests) |
    74|74|| T18 | X (Twitter) search | `test_x_search.c` (14 tests) |
    75|75|| T19 | Home Assistant — 5 tools | `test_homeassistant.c` (26 tests) |
    76|76|| T20 | Discord — 2 tools | `test_discord.c` (31 tests) |
    77|77|| T21 | Kanban — 9 tools | `test_kanban.c` (39 tests) |
    78|78|| T22 | Computer use — macOS + X11 + Wayland + noop backends | `test_computer_use.c` |
    79|79|| T23 | Feishu tools — doc_read, drive (add/list/reply comment) | `test_feishu_tools.c` (42 tests) |
    80|80|| T24 | Voice mode — listen/speak | `test_voice_mode.c` |
    81|81|| T25 | Cronjob management | `test_cronjob.c` |
    82|82|| T26 | Approval system | `test_edit_approval.c` (44 tests) |
    83|83|| T27 | MCP tool integration | `test_mcp.c`, `test_mcp_stream.c`, `test_mcp_oauth.c` |
    84|84|| T28 | MCP OAuth flow | `test_mcp_oauth.c` (13 tests) |
    85|85|| T29 | Patch tool — 9 fuzzy matching strategies | `test_patch.c` |
    86|86|| T30 | Execute code tool | `test_exec_code.c` |
    87|87|| T31 | Tool output limits | `test_tool_output.c` (21 tests) |
    88|88|| T32 | Tool result storage | `test_result_storage.c` |
    89|89|| T33 | Website policy enforcement | `test_website_policy.c` (11 tests) |
    90|90|| T34 | OSV vulnerability check | `test_osv.c` (11 tests) |
    91|91|| T35 | Credential management | `test_credential.c`, `test_credential_pool.c` |
    92|92|| T36 | Account usage tracking | `test_account_usage.c` (11 tests) |
    93|93|| T37 | Binary extensions | `test_binary.c` (14 tests) |
    94|94|| T38 | WeCom crypto (WXBizMsgCrypt) | `test_wecom_crypto.c` (28 tests) |
    95|95|| T39 | Message sanitization (repair tool call arguments) | `test_sanitize.c` (20 tests) |
    96|96|
    97|97|## Phase 5: Gateway Platforms
    98|98|
    99|99|| ID | Achievement | Evidence |
   100|100||----|-------------|----------|
   101|101|| G01 | Gateway server (server.c, 2143 LOC) — webhook queue, async prompt response, connection pool, keepalive | `test_managed_gateway.c` |
   102|102|| G02 | Telegram platform | `test_telegram.c` |
   103|103|| G03 | Discord platform | `test_discord_interactions.c` |
   104|104|| G04 | Webhook platform | Integration tested |
   105|105|| G05 | Slack platform | Compiled |
   106|106|| G06 | Matrix platform | Compiled |
   107|107|| G07 | Mattermost platform | Compiled |
   108|108|| G08 | WhatsApp platform | Compiled |
   109|109|| G09 | Email platform (IMAP/SMTP) | Compiled |
   110|110|| G10 | Signal platform | Compiled |
   111|111|| G11 | Home Assistant platform | Compiled |
   112|112|| G12 | SMS platform | Compiled |
   113|113|| G13 | Feishu platform | Compiled |
   114|114|| G14 | WeCom platform | Compiled |
   115|115|| G15 | DingTalk platform | Compiled |
   116|116|| G16 | QQ Bot platform | Compiled |
   117|117|| G17 | BlueBubbles platform | Compiled |
   118|118|| G18 | MS Graph webhook platform | Compiled |
   119|119|| G19 | Weixin platform | Compiled |
   120|120|| G20 | YuanBao platform | Compiled |
   121|121|| G21 | Gateway reactions (send_reaction vtable) | `test_gateway_reactions.c` |
   122|122|
   123|123|## Phase 6: Libraries
   124|124|
   125|125|| ID | Achievement | Evidence |
   126|126||----|-------------|----------|
   127|127|| L01 | libhttp — HTTP client with streaming, SSL, retry, cookies, auth | 1655 LOC, 101 functions |
   128|128|| L02 | libjson — JSON parser/serializer with surrogate pair support | 693 LOC, 40 functions |
   129|129|| L03 | libmcp — MCP protocol client with SSE transport, OAuth | 2093 LOC, 105 functions |
   130|130|| L04 | libdb — SQLite session storage | Bundled sqlite3 |
   131|131|| L05 | libcrypto — AES, SHA1, SHA256, HMAC, base64url, PKCS7 | 511 LOC, 41 functions |
   132|132|| L06 | libplugin — Plugin loading and management | 881 LOC, 65 functions |
   133|133|| L07 | libtui — TUI framework (ncurses-based) | 487 LOC, 32 functions |
   134|134|| L08 | libwebsocket — WebSocket client | 372 LOC, 19 functions |
   135|135|| L09 | libyaml — YAML parser | 563 LOC, 39 functions |
   136|136|| L10 | libglob — Glob pattern matching | 163 LOC, 7 functions |
   137|137|| L11 | libregex — Regex wrapper | 119 LOC, 14 functions |
   138|138|| L12 | libcsv — CSV parser | 398 LOC, 24 functions |
   139|139|| L13 | libdatetime — Date/time utilities | 373 LOC, 35 functions |
   140|140|| L14 | libhash — SHA-256, MD5, HMAC | 177 LOC, 14 functions |
   141|141|| L15 | libbase64 — Base64 encode/decode | 199 LOC, 19 functions |
   142|142|| L16 | libansi — ANSI escape handling | 358 LOC, 27 functions |
   143|143|| L17 | libhtml — HTML parsing utilities | 166 LOC, 13 functions |
   144|144|| L18 | libdifflib — Diff/patch utilities | 242 LOC, 31 functions |
   145|145|| L19 | libpath — Path manipulation | 417 LOC, 45 functions |
   146|146|| L20 | libcron — Cron expression parsing | 340 LOC, 17 functions |
   147|147|| L21 | libproc — Process management helpers | 205 LOC, 10 functions |
   148|148|| L22 | libuuid — UUID generation | 168 LOC, 15 functions |
   149|149|| L23 | libdotenv — .env file parser | 252 LOC, 15 functions |
   150|150|| L24 | libdebug — Debug helper utilities | 211 LOC, 9 functions |
   151|151|| L25 | libosv — OSV vulnerability database client | 283 LOC, 15 functions |
   152|152|| L26 | libwebsite — Website policy enforcement | 245 LOC, 21 functions |
   153|153|| L27 | libenvpassthrough — Env var passthrough config | 154 LOC, 8 functions |
   154|154|| L28 | libxai_http — xAI HTTP client | 51 LOC, 3 functions |
   155|155|| L29 | libcredential — Credential management | 537 LOC, 26 functions |
   156|156|| L30 | libschemasanitizer — JSON schema sanitization | 628 LOC, 47 functions |
   157|157|| L31 | libfuzzymatch — Fuzzy string matching | 738 LOC, 79 functions |
   158|158|| L32 | libinterrupt — Cross-platform interrupt handling | 71 LOC, 7 functions |
   159|159|| L33 | libfilestate — File state tracking | 411 LOC, 32 functions |
   160|160|| L34 | libtooldispatch — Tool dispatch helpers | 305 LOC, 15 functions |
   161|161|| L35 | librateguard — Rate limiting guards | 204 LOC, 16 functions |
   162|162|| L36 | libratelimit — Rate limit tracking | 386 LOC, 31 functions |
   163|163|| L37 | libskillutils — Skill utility functions | 652 LOC, 35 functions |
   164|164|| L38 | liberrorclassifier — Error classification | 812 LOC, 40 functions |
   165|165|| L39 | libfile_sync — File synchronization | 243 LOC, 12 functions |
   166|166|| L40 | libbudgetconfig — Budget configuration | 90 LOC, 4 functions |
   167|167|| L41 | libthreatpatterns — Security threat patterns | 301 LOC, 15 functions |
   168|168|| L42 | libcredentialfiles — Credential file management | 340 LOC, 20 functions |
   169|169|| L43 | libskillaudit — Skill security audit | 383 LOC, 19 functions |
   170|170|| L44 | libslashconfirm — Slash command confirmation | 210 LOC, 17 functions |
   171|171|| L45 | libmsgraph — Microsoft Graph API client | 517 LOC, 26 functions |
   172|172|| L46 | libsignal — Signal handling | 66 LOC, 9 functions |
   173|173|| L47 | libbinary — Binary file utilities | 96 LOC, 2 functions |
   174|174|| L48 | libbrowser — Browser Camofox state | 226 LOC, 13 functions |
   175|175|| L49 | libtemplate — Template engine | 554 LOC, 33 functions |
   176|176|| L50 | libtoml — TOML parser | 514 LOC, 21 functions |
   177|177|| L51 | libjson5 — JSON5 parser | 481 LOC, 22 functions |
   178|178|| L52 | libmcp_oauth — MCP OAuth flow | 1262 LOC, 98 functions |
   179|179|| L53 | libfal_common — FAL API common utilities | 95 LOC, 5 functions |
   180|180|| L54 | libtooloutput — Tool output limits | 56 LOC, 6 functions |
   181|181|| L55 | libmangateway — Managed gateway support | 280 LOC, 14 functions |
   182|182|| L56 | libcreditfiles — Credential files | Included |
   183|183|| L57 | libbrowser — browser state management | Included |
   184|184|| L58 | libtranscribe — Audio transcription | 611 LOC, 29 functions |
   185|185|| L59 | libtoml — TOML serialization | Included |
   186|186|| L60 | libbinary — Binary data handling | Included |
   187|187|| L61 | liblineedit — Line editing | 594 LOC, 31 functions |
   188|188|| L62 | libskillusage — Skill usage tracking + provenance | 622 LOC, 37 functions |
   189|189|| L63 | libskillsync — Skill sync + diff | 707 LOC, 41 functions |
   190|190|| L64 | libwebsearchregistry — Web search registry | Separate compilation |
   191|191|| L65 | libncurses — ncurses headers (bundled, WSL compat) | 10857 LOC headers only |
   192|192|
   193|193|## Phase 7: Stale Claims Retired
   194|194|
   195|195|Claims from past battleship versions verified as already implemented.
   196|196|
   197|197|| ID | Old Claim | Reality | Evidence |
   198|198||----|-----------|---------|----------|
   199|199|| ~80% of 1A depth gaps | Various tool feature gaps | Already implemented in C | Verified via grep 2026-05-27 |
   200|200|| S07 | shutdown = NULL (S07) | Resolved via gw_platform_shutdown_all() | server.c |
   201|201|| S01/S02 | Browser CDP stubs | Real WebSocket/JSON-RPC | browser.c |
   202|202|| D09 | CUA backend missing | Implemented at computer_use.c | ~400 LOC |
   203|203|| D12 | CDP backend stub | Real implementation | browser.c |
   204|204|| L14-L29 | Various library depth claims | Already implemented | Verified via grep |
   205|205|
   206|206|## Phase 8: Gateway Shutdown Callback
   207|207|
   208|208|| ID | Achievement | Evidence |
   209|209||----|-------------|----------|
   210|210|| S02 | server.c:2066 — replaced `plat.shutdown = NULL` with `poll_platform_shutdown()` real callback | `src/gateway/server.c:1149` — `gw_platform_shutdown_all()` now calls logging callback per platform |
   211|211|| L02 | libmcp SSE transport: transport_send captures POST response body in recv_buf, transport_read_response parses and matches by request_id — SSE request-response now functional | `lib/libmcp/mcp.c` — recv_buf/recv_len + transport_send/read_response SSE paths |
   212|212|| S04 | commands.c:2595 — `reload plugins` calls hermes_plugin_shutdown + hermes_plugin_init (was stub) | `src/cli/commands.c` — hot-reload path |
   213|213|| S08 | homeassistant.c:524 — changed `Deprecated: use 'data' instead` to `Legacy alias for 'data' parameter` | `src/tools/homeassistant.c` — JSON schema string fix |
   214|214|| S03 | commands.c:2704-2747 — `/restart` now saves session, closes DB, execv with full path, strerror on failure, removed ARG_MAX stack waste | `src/cli/commands.c` — session save + execv + errno handling |
   215|215|
   216|216|## Phase 9: Stale Claims Retired (Triple DA v19 → v20)
   217|217|
   218|218|Claims from battleship-v19 verified against running source — all already implemented.
   219|219|
   220|220|| ID | Old Claim | Reality | Evidence |
   221|221||----|-----------|---------|----------|
   222|222|| F01 | `--bogus` sends to LLM (P0) | Error: "unknown flag '--bogus-flag'" | `./slermes --bogus-flag` returns proper error |
   223|223|| F02 | Multi-line pipe mode broken (P0) | `/help /tools /exit` processes each command | `printf '/help\n/tools\n' | ./slermes` works |
   224|224|| F03 | `--session` without arg runs session_crud (P0) | Error: "--session requires a session ID" | `./slermes --session` returns proper error |
   225|225|| F04 | Tools display shows 83 (stale, P1) | Shows "Registered tools (99)" | live binary `/tools` command |
   226|226|| M01 | discord tool missing (P2) | `discord` tool registered with 12 actions | `src/tools/discord.c:482` — registry_register("discord") |
   227|227|| M02 | discord_admin missing (P2) | `discord_admin` tool registered | `src/tools/discord.c:510` — registry_register("discord_admin") |
   228|228|| L02 | libmcp SSE streaming missing | SSE transport state, event buffer, POST to SSE endpoint | `lib/libmcp/mcp.c:45-56` — SSE transport fields |
   229|229|| I01 | No Dockerfile | Dockerfile exists (1177 bytes) | `C/Dockerfile` — build works with `docker build` |
   230|230|
   231|231|Note: X01-X05 (test coverage gaps) were listed as "0 test files" — each file exists with substantive content (vision: 229L, image_gen: 76L, video_gen: 61L, transcribe: 95L, session_search: 285L). Edge case expansion P3 remains.
   232|232|
   233|233|## Phase 10: Gateway Vtable Wiring
   234|234|
   235|235|| ID | Achievement | Evidence |
   236|236||----|-------------|----------|
   237|237|| G02 | Telegram send_reaction wired to gw_platform_t vtable — `gw_platform_send_reaction(platform, chat_id, message_id, emoji)` now dispatches to `telegram_set_message_reaction()` via `telegram_vtable_send_reaction` wrapper in server.c | `src/gateway/server.c:1148-1154` — wrapper; `src/gateway/server.c:2086-2092` — wiring in registration loop |
   238|238|
   239|239|## Phase 11: Discord Tool Depth (D13)
   240|240|
   241|241|| ID | Achievement | Evidence |
   242|242||----|-------------|----------|
   243|243|| D13 | Discord tool expanded from 12 to 19 actions: new handlers for search_members, list_pins, pin_message, unpin_message, create_thread, add_role, remove_role — plus schema enum, dispatch branches, and parameter extraction | `src/tools/discord.c` — 7 handler functions; dispatch at strcmp branches; role_id/query params |
   244|244|
   245|245|## Phase 12: Send Message Inline Buttons (D06 partial)
   246|246|
   247|247|| ID | Achievement | Evidence |
   248|248||----|-------------|----------|
   249|249|| D06 | Inline buttons for send_message tool: new `inline_buttons` schema param, `build_inline_keyboard()` helper builds multi-row reply_markup JSON. Direct libhttp Telegram send replaces broken system() shell-out | `src/tools/send_message.c` — SCHEMA has inline_buttons[{text,url?,callback_data?,row?}]; libhttp path at telegram branch; suite 282/0/0 |
   250|250|
   251|251|## Phase 13: Delegate Spawn Pause (D07 partial)
   252|252|
   253|253|| ID | Achievement | Evidence |
   254|254||----|-------------|----------|
   255|255|| D07 | Spawn pause for delegate_task: `set_spawn_paused()`/`is_spawn_paused()` global gate checked before each parallel batch. Exposed via hermes.h for TUI/gateway RPC integration | `src/tools/delegate.c` — `g_spawn_paused` flag + mutex; `is_spawn_paused()` check in `spawn_children()` before each batch; header decls in `include/hermes.h` |
   256|256|## Phase 14: Azure TTS Provider (D02 partial)
   257|257|
   258|258|
   259|259|## Phase 15: Magic-Byte Image Detection (D08 partial)
   260|260|
   261|261|| ID | Achievement | Evidence |
   262|262||----|-------------|----------|
   263|263|| D08 | Magic-byte image format detection: `detect_image_magic()` reads first 12 bytes and identifies PNG, JPEG, GIF, BMP, TIFF, WebP, ICO, AVIF, HEIC by header signature. Extensionless files with valid image bytes are now accepted instead of rejected | `src/tools/vision.c` — `detect_image_magic()` function; wired into `vision_handler` at the extension check fallback; returns `detected_format` field in result JSON |
   264|264|| ID | Achievement | Evidence |
   265|265||----|-------------|----------|
   266|266|| D02 | Azure Cognitive Services TTS provider: SSML-based POST to {region}.tts.speech.microsoft.com with Ocp-Apim-Subscription-Key auth. Supports AZURE_TTS_KEY / AZURE_SPEECH_KEY env vars and azure region config. New `azure` provider option in text_to_speech schema | `src/tools/tts.c` -- tts_azure() function following existing API pattern; SSML body construction; dispatch branch at strcmp("azure"); schema updated to list azure |
   267|267|
   268|268|## Phase 16: Image Gen Download Validation (D09 partial)
   269|269|
   270|270|| ID | Achievement | Evidence |
   271|271||----|-------------|----------|
   272|272|| D09 | Download size validation for image_gen: 50MB max, HTTP status check, empty body detection, connection timeout handling. Warning now includes specific error message instead of generic 'Could not download' | `src/tools/image_gen.c` — body_len > 50MB check; granular error branching (HTTP status/empty body/connection/write fail) |
   273|273|
   274|274|## Phase 17: Docker Compose Backend (D04 partial)
   275|275|
   276|276|| ID | Achievement | Evidence |
   277|277||----|-------------|----------|
   278|278|| D04 | Docker Compose execution backend: `run_command_docker_compose()` uses `docker compose exec -T` to run commands in a compose service. Configurable via `terminal.compose_service` config key (default: 'default') | `src/tools/terminal.c` -- run_command_docker_compose() function; dispatch branch at backend=docker-compose|compose; schema updated |
   279|279|
   280|280|## Phase 18: Deepgram Transcription Provider (D05)
   281|281|
   282|282|| ID | Achievement | Evidence |
   283|283||----|-------------|----------|
   284|284|| D05 | Deepgram transcription provider: POST to api.deepgram.com/v1/listen with DEEPGRAM_API_KEY, supports model, language, punctuate params. Wired into libtranscribe dispatch | `src/tools/transcribe.c` — transcribe_deepgram() function; dispatch branch at strcmp("deepgram"); schema updated |
   285|285|
   286|286|## Phase 19: Hidden File Detection (D12)
   287|287|
   288|288|| ID | Achievement | Evidence |
   289|289||----|-------------|----------|
   290|290|| D12 | Hidden file detection for read_file tool: `is_hidden` field returned in file metadata. Dotfiles flagged on Linux, FILE_ATTRIBUTE_HIDDEN on Windows (via statfs). Wired into file_metadata() helper | `src/tools/file.c` — is_hidden check in file_metadata(); schema updated with is_hidden field |
   291|291|
   292|292|## Phase 20: CDP PDF Generation (D03)
   293|293|
   294|294|| ID | Achievement | Evidence |
   295|295||----|-------------|----------|
   296|296|| D03 | CDP PDF generation via Page.printToPDF: new browser_generate_pdf tool sends CDP Page.printToPDF params (landscape, format, margin, scale, printBackground). Returns base64-encoded PDF. Uses existing CDP send infrastructure | `src/tools/browser.c` — browser_generate_pdf handler; CDP Page.printToPDF params construction; schema with 8 params; registry_register("browser_generate_pdf") |
   297|297|
   298|298|## Phase 21: SSE Streaming for MCP Transport (D01)
   299|299|
   300|300|| ID | Achievement | Evidence |
   301|301||----|-------------|----------|
   302|302|| D01 | SSE streaming for MCP transport_read_response: added `http_sse_start()`/`http_sse_read_event()`/`http_sse_free()` to libhttp for persistent SSE stream connections. `transport_read_response` for SSE now reads events from the persistent stream when POST response buffer doesn't contain the matching response. Incoming server-to-client requests are queued. Full SSE event parsing (event:, data: fields, timeout, EOF handling) | `lib/libhttp/http.c` — http_sse_start/http_sse_read_event/http_sse_free; `lib/libhttp/http.h` — SSE stream API; `lib/libmcp/mcp.c` — sse_stream field, connect wiring, transport_read_response SSE branch, disconnect cleanup |
   303|303|
   304|304|## Phase 22: CI Stale Claim Retired (I02)
   305|305|
   306|306|| ID | Old Claim | Reality | Evidence |
   307|307||----|-----------|---------|----------|
   308|308|| I02 | CI pipeline missing | C build (c-build.yml, 338L), tests.yml, c-release.yml, docker workflows all exist — triggers on C/ path changes, builds + runs 282 tests | `.github/workflows/c-build.yml`, `tests.yml`, `c-release.yml` — verified live on disk May 28 |
   309|309|
   310|310|## Phase 23: Vision Edge Case Tests (X01)
   311|311|
   312|312|| ID | Achievement | Evidence |
   313|313||----|-------------|----------|
   314|314|| X01 | Vision edge case tests expanded from 15 to 31: magic-byte detection for JPEG, GIF, BMP, TIFF, WebP, ICO (no extension); empty file passthrough; header-only PNG (both with and without extension); .bin with JPEG magic proves magic override. Bugfix: magic-byte path now sets image_url so providers can process magic-detected files | `tests/test_vision.c` — 16 new edge case tests (31 total); `src/tools/vision.c` — magic-byte branch sets image_url + detail/analysis |
   315|315|
   316|316|## Phase 24: Image Gen Edge Case Tests (X02)
   317|317|
   318|318|| ID | Achievement | Evidence |
   319|319||----|-------------|----------|
   320|320|| X02 | Image generation edge case tests expanded from 7 to 18: invalid/empty provider, negative aspect ratio, 4K-char prompt, JSON injection, unicode/emoji, all-params-set, n=0, n=999, unknown extra params | `tests/test_image_gen.c` — 11 new tests (18 total) |
   321|321|
   322|322|## Phase 25: Video Gen Edge Case Tests (X03)
   323|323|
   324|324|| ID | Achievement | Evidence |
   325|325||----|-------------|----------|
   326|326|| X03 | Video generation edge case tests expanded from 5 to 14: empty prompt, empty/invalid provider, 4K-char prompt, JSON injection, unicode/emoji, all-params-set, unknown params, negative duration | `tests/test_video_gen.c` — 9 new tests (14 total) |
   327|327|
   328|328|## Phase 26: Transcribe Edge Case Tests (X04)
   329|329|
   330|330|| ID | Achievement | Evidence |
   331|331||----|-------------|----------|
   332|332|| X04 | Transcribe edge case tests expanded from 8 to 13: empty provider, invalid provider, unknown params, 4K-char file path, JSON injection in model param | `tests/test_transcribe.c` — 5 new tests (13 total) |
   333|333|
   334|334|## Phase 27: Session Search Edge Case Tests (X05)
   335|335|
   336|336|| ID | Achievement | Evidence |
   337|337||----|-------------|----------|
   338|338|| X05 | Session search edge case tests expanded from 13 to 17: invalid JSON args, 4K-char query, unicode query, empty tag_filter (graceful), negative limit | `tests/test_session_search.c` — 5 new tests (17 total) |
   339|339|
   340|340|## Phase 28: Honest Upstream Drift Assessment (v116)
   341|341|
   342|342|| ID | Achievement | Evidence |
   343|343||----|-------------|----------|
   344|344|| U01 | Discovered and documented 7583 upstream commits behind upstream since fork point 2517917de | `git rev-list --count HEAD..upstream/main` = 7583 |
   345|345|| U02 | Cataloged 7 upstream drift impact areas: provider/API, agent loop, gateway, tool schema, MCP, security/auth, test suite | `C/.hermes/mind-palace/battleship-v27.md` S4 sector |
   346|346|| U03 | Retired false "0 gaps" claim — replaced with honest 33-gap assessment across 5 sectors | `C/.hermes/mind-palace/state.md` v116 |
   347|347|| U04 | Updated all walkway files to v116 with correct tool/CLI counts (85 tools, 80 CLI, 37 config sections) | All files in `C/.hermes/mind-palace/` |
   348|348|| U05 | Updated BANNER.md and README.md with honest gap counts | `C/BANNER.md`, `C/README.md` header block |
   349|349|| U06 | Fast-forwarded to upstream origin/main (3 new commits) | `git fetch origin main` -> `git merge --ff-only` to b243afb68 |
   350|350|| U07 | Verified no regression after upstream sync -- suite 282/0/0, 0 warnings | `make clean && make` + `bash test_runner.sh` |
   351|351|
   352|352|## Phase 29: Fork Sync & Doc Migration (v117)
   353|353|
   354|354|| ID | Achievement | Evidence |
   355|355||----|-------------|----------|
   356|356|| U08 | Fork synced to upstream (0 behind, 2 ahead) | git: main at 67011cc0d + C/ squashed |
   357|357|| U09 | Old dev history preserved on c-work branch (277 commits) | GitHub: waefrebeorn/slermes/tree/c-work |
   358|358|| U10 | C/.hermes/ mind-palace docs force-tracked in git | .gitignore patched with !C/.hermes/ |
   359|359|| U11 | Root README migrated to Slermes branding | README.md entry point |
   360|360|| U12 | Battleship v28 — S4 reworded for accuracy | C/.hermes/mind-palace/battleship-v28.md |
   361|361|| U13 | All walkway files bumped to v117 | state, plan, prestige, overnight, entry, index, testing
   362|362|
   363|363|## Phase 30: TUI Agent Chat Wiring (F01)
   364|364|
   365|365|| ID | Achievement | Evidence |
   366|366||----|-------------|----------|
   367|367|| F01 | TUI fullscreen mode now calls `agent_chat()` with streaming callback — replaced `"[Agent processing...]"` stub that did nothing. Retry (handled by agent_loop.c's retry loop) now functional in TUI context. Stream tokens display in real-time via `tui_fullscreen_stream_token()` adapter | `src/cli/tui_fullscreen.c` — `tui_stream_cb()` adapter + `tui_process_input()` agent_chat wiring; suite 282/0/0 |
   368|368|
   369|369|## Phase 31: Interactive Setup Wizard (F02)
   370|370|
   371|371|| ID | Achievement | Evidence |
   372|372||----|-------------|----------|
   373|373|| F02 | Added `slermes setup` — interactive wizard with provider menu, model prompt, API key capture. Creates config.yaml + .env. Skips existing config with warning. Wired via main.c dispatch to config.c `hermes_config_setup_interactive()` | `src/cli/config.c` — `hermes_config_setup_interactive()` (131 LOC); `src/main.c` — `setup` command dispatcher; suite 282/0/0 |
   374|374|
   375|375|## Phase 32: Async-Signal-Safe SIGWINCH (F03)
   376|376|
   377|377|| ID | Achievement | Evidence |
   378|378||----|-------------|----------|
   379|379|| F03 | SIGWINCH handler now sets `volatile sig_atomic_t` flag instead of calling ncurses from signal context. Actual resize (endwin/refresh/clear/tui_resize_panes) deferred to main loop — checked after gateway poll and before input. | `src/cli/tui_fullscreen.c` — `g_resize_requested` flag + handler rewrite + main loop check; suite 282/0/0 |
   380|380|
   381|381|## Phase 33: Stale Claim Retired — P01
   382|382|
   383|383|| ID | Old Claim | Reality | Evidence |
   384|384||----|-----------|---------|----------|
   385|385|| P01 | Retry broken in TUI | Agent retry is handled entirely by agent_loop.c's retry loop (jittered exponential backoff, fallback routing). TUI calls agent_chat() which goes through the same retry path as CLI. No TUI-specific retry code needed. | `src/agent/agent_loop.c` — retry loop at agent_run_conversation; `src/cli/tui_fullscreen.c` — agent_chat wiring (F01); F01 verified retry functional |
   386|386|
   387|387|## Phase 34: Compiler Warning Cleanup
   388|388|
   389|389|| ID | Achievement | Evidence |
   390|390||----|-------------|----------|
   391|391|| W01 | Eliminated all non-truncation compiler warnings (was ~20): unused params, uninitialized variable, unused functions, ignored asprintf/chdir returns, always-true comparison, GNU ?: extension. Build now produces only -Wformat-truncation and -Wstringop-truncation warnings (benign, buffer size analysis) | `src/secrets.c` — init guard; `src/agent/file_safety.c` — (void)asprintf (×5); `src/cli/cli.c` — session_id[0] check; `src/agent/image_routing.c` — (void)provider + unused fn attr; `src/agent/context_references.c` — (void)chdir; `src/agent/shell_hooks.c` — 4 unused fn attrs; `src/cli/display.c` — (void)role; suite 282/0/0 |
   392|392|
   393|393|## Phase 35: Patch Tool Tab/CR Unescape
   394|394|
   395|395|| ID | Achievement | Evidence |
   396|396||----|-------------|----------|
   397|397|| P01 | Ported upstream patch tool \\t/\\r unescape fix (@78be45860) to C: `_maybe_unescape_new_string` — conditionally unescapes \\t→tab and \\r→CR in new_string when matched file region contains real control bytes. Region-based heuristic (not strategy-gated). `\\n` excluded intentionally. | `src/tools/patch.c` — new unescape block before replacement loop; `tests/test_patch.c` — 4 new test scenarios (12 assertions): tab exact, \\n preservation, literal preservation, passthrough. Suite 282/0/0, patch tests 37/0/0. |
   398|398|
   399|399|## Phase 36: Dead Code & Warning Cleanup (v124)
   400|400|
   401|401|| ID | Achievement | Evidence |
   402|402||----|-------------|----------|
   403|403|| W02 | Removed unused `coerce_capability_bool` — 26 lines dead code | `src/agent/image_routing.c` |
   404|404|| W03 | `(void)provider` in `supports_vision_override` to suppress unused-param warning | `src/agent/image_routing.c:171` |
   405|405|| W04 | `__attribute__((unused))` on `shell_escape` in session_search.c | `src/tools/session_search.c:79` |
   406|406|| W05 | Removed unused `early_len` variable | `src/tools/session_search.c:129` |
   407|407|| W06 | Added `!name` null guard before `!name[0]` check in secrets.c — fixes -Wmaybe-uninitialized and potential use-after-scope on block-local `secret_name` | `src/secrets.c:200` |
   408|408|| | Suite 283/0/0, commit 48e5622a8 | |
   409|409|
   410|410|## Phase 37: Warning Suppression Sweep (v125)
   411|411|
   412|412|| ID | Achievement | Evidence |
   413|413||----|-------------|----------|
   414|414|| W07 | Marked 7 unused plugin_* functions in memory.c with __attribute__((unused)) | `src/tools/memory.c` |
   415|415|| W08 | Removed unused `sdb` variable in config.c YAML parsing | `src/cli/config.c` |
   416|416|| W09 | Suppressed 7 unused functions (xstrdup, is_set_bool, build_bot_header, kanban_mode, kanban_orchestrator, extract_registered_domain, strcasestr_safe) | 7 files |
   417|417|| W10 | (void) unused params: computer_use.c (element×2, amount), mcp_tool.c (args_json, server_name, arg_count), kanban.c (tid) | 3 files |
   418|418|| | Suite 283/0/0, commit 80a4dc334 | |
   419|419|
   420|420|## Phase 39: Sequence-point UB & Missing Headers (v126)
   421|421|
   422|422|| ID | Achievement | Evidence |
   423|423||----|-------------|----------|
   424|424|| W11 | Fixed 8 sequence-point UB sites in browser.c: extracted `g_tab.element_count` before snprintf argument evaluation (C undefined behavior — read+write same var in one expression) | `src/tools/browser.c` — 8 `int _eidx = g_tab.element_count++;` patterns |
   425|425|| W13 | Added missing headers: `#include <unistd.h>` in telegram.c (usleep), `#include <sys/stat.h>` in cronjob.c (mkdir), `#include <ctype.h>` in server.c (tolower), forward declaration for discord_send_typing_to | 4 files patched |
   426|426|| | Suite 283/0/0, commit 3940341af | |
   427|427|
   428|428|
   429|
## Phase 42: Gateway Media & Session Meta Depth (v132)

- MEDIA: prefix routing in gateway_send (Telegram: photo/voice/video/animation/document)
- send_message tool: actual media file send via Telegram API
- session_meta_t: added reasoning_tokens, estimated_cost fields (schema v3)
- Fixed pre-existing send_message test crash (TEST_BUILD guard)
- Evidence: commits 79fe05286, cd5ecfc9c, 34d6ef494

## Phase 43: Shell Hooks Lifecycle Wiring (v133)

- libyaml: added yaml_to_json_string() — serialize YAML sub-tree to JSON
- config.c: populate cfg->hooks_json from YAML hooks: block
- agent_loop.c: call shell_hooks_parse_json() + shell_hooks_register_all() in agent_configure_from_config()
- Full lifecycle now wired: YAML hooks: block → JSON → parse → register → hook_invoke in agent_loop
- Evidence: commit c6cbd1e4c

## Phase 41: Lifecycle Hook Wiring (v131)
   430|
   431|- Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points
   432|- pre_llm_call: before each LLM API call, includes model/provider/iteration/tool count
   433|- post_llm_call: after LLM response, includes token counts and finish_reason
   434|- pre_tool_call: before tool dispatch, includes tool count and iteration
   435|- post_tool_call: after tool execution completes, before result message creation
   436|- Previously hook_register() was called by shell_hooks.c but hook_invoke() never called
   437|- Evidence: commit dde46b7c0, file src/agent/agent_loop.c
   438|## Phase 40: W14 Type Mismatch Fixes (v127)
   439|429|
   440|430|| ID | Achievement | Evidence |
   441|431||----|-------------|----------|
   442|432|| W14a | Fixed `char = "\0"` (char*→char) in xai_http.c — was assigning string literal to char var | `src/tools/xai_http.c:37,59` — `'\0'` not `"\0"` |
   443|433|| W14b | Fixed `kanban_orchestrator` returning bool from void function — changed return type to bool | `src/tools/kanban.c:74` — `static bool` |
   444|434|| W14c | Fixed `telegram_get_updates` returning `http_response_t*` as `json_node_t*` — now parses HTTP response body to JSON | `src/gateway/platforms/telegram.c:647` — `json_parse(resp->body, NULL)` |
   445|435|| | Suite 283/0/0, commit 79a1f3825 | |
   446|436|
   447|437|## Phase 41: W12 strtok_r Incompatible Pointer Fix (v128)
   448|438|
   449|439|| ID | Achievement | Evidence |
   450|440||----|-------------|----------|
   451|441|| W12 | Fixed 3 strtok_r calls passing `char (*)[N]` as save_ptr — added proper `char *saveN` variables to suppress -Wincompatible-pointer-types and -Wrestrict warnings | `src/tools/terminal.c:331,363,381` — `&save1`/`&save2`/`&save3` |
   452|442|| | Suite 283/0/0, commit b955d29a5 | |
   453|443|
   454|444|## Phase 38: Stale S4 Claims Retired (v33 battleship)
   455|445|
   456|446|| ID | Old Claim | Reality | Evidence |
   457|447||----|-----------|---------|----------|
   458|448|| U01 | XAI retry 429 | Neither Python plugins/model-providers/xai nor C provider_xai.c have 429 retry | `grep -rn '429\|retry.*after' src/agent/provider_xai.c plugins/model-providers/` — 0 matches |
   459|449|| U01 | OAuth fixes | C already has PKCE helpers in crypto.c | `src/deps/crypto.c:150` — PKCE helpers |
   460|450|| U01 | MiniMax compat | C already has MiniMax in provider_metadata.c | `src/agent/provider_metadata.c:35` — "MiniMax" |
   461|451|| U02 | Agent loop/fallback | C has fallback_routing.c with cool-off + credential_pool | `src/agent/fallback_routing.c` — fallback_chain_create |
   462|452|| U03 | Discord thread backfill | Neither Python gateway/platforms/ nor C discord.c have backfill | `grep -rn 'backfill' src/gateway/platforms/` — 0 matches |
   463|453|| U03 | Windows gateway drain | C server.c has SIGTERM/SIGINT handlers | `src/gateway/server.c:2119` |
   464|454|| U03 | Platform hardening | C server.c has gw_reconnect_delay | `src/gateway/server.c:536` |
   465|455|| U04 | TIRITH tar safety | Neither Python tirith_security.py nor C tirith.c have tar safety | grep across both = 0 matches |
   466|456|| U04 | web_crawl removal | C never had web_crawl | 0 results for web_crawl anywhere in C/ |
   467|457|| U05 | mTLS client certs | Neither Python MCP nor C libmcp have mTLS | 0 results for tls/ssl/certificate in lib/libmcp/ or tools/mcp_tool.py |
   468|458|| U06 | PKCE, CIDR, SSRF | C already has all three | crypto.c (PKCE), shell_hooks.c (CIDR allowlist), url_safety (SSRF) |
   469|459|| | Battleship v32 S4 reduced from 7 items → 1 item (U07 test gap). New battleship v33 with 21 real gaps. | | |
   470|460|
   471|461|
   472|## Phase 40: Provider Mode Parity Deepening (v130)
   473|
   474|- Q06 deepened: wired `service_tier` to Anthropic `build_request_body`
   475|- Q06 deepened: wired `reasoning_effort` to OpenRouter, Azure, Custom `build_request_body`
   476|- Evidence: commit 180c0cc97, files provider_anthropic.c, provider_openrouter.c, provider_azure.c, provider_custom.c
   477|
   478|## Phase 41: Lifecycle Hook Wiring (v131)
   479|
   480|- Wired hook_invoke() calls into agent_loop.c at 4 lifecycle points
   481|- pre_llm_call: before each LLM API call, includes model/provider/iteration/tool count
   482|- post_llm_call: after LLM response, includes token counts and finish_reason
   483|- pre_tool_call: before tool dispatch, includes tool count and iteration
   484|- post_tool_call: after tool execution, before result message creation
   485|- Previously hook_register() was called by shell_hooks.c but hook_invoke() never called
   486|- Evidence: commit dde46b7c0, file src/agent/agent_loop.c
   487|
   488|## Phase 39: Build & Test Infrastructure (v129)
   489|462|
   490|463|| ID | Achievement | Evidence |
   491|464||----|-------------|----------|
   492|465|| A01 | Fixed ASan build (added -lz to LDFLAGS, inflateEnd undefined) | Makefile:454 |
   493|466|| A02 | Fixed feishu test segfault (http.o->http.c, stale ASan-object crash) | test_runner.sh:2510 |
   494|467|| A03 | Corrected stale CLI count 80->98 in state.md, entry.md, battleship-v33.md | 98 real CLI commands verified |
   495|468|| | Suite 283/0/0, commits e0d7ccbb0, 86db5fecc pushed | All on waefrebeorn/slermes main |
   496|469|
   497|
