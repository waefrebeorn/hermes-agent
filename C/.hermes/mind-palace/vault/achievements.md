     1|# Slermes C Translation — Vault of Achievements
     2|
     3|All completed work archived here. Clears the active gap list for fresh battleship generation.
     4|Last updated: 2026-05-26
     5|
     6|## Phase 12: Battleship-v15 Resolved Items (2026-05-26)
     7|
     8|| ID | Description | Sector | Evidence |
     9||----|-------------|--------|----------|
    10|| T21 | Approval gateway prompt — gateway callback pattern, Telegram short-poll for async response, cross-platform condvar | 1A | approval.c + server.c — approval_set_gateway_send/wait, gw_approval_set_context, gw_approval_wait_response |
    11|| T23 | Patch V4A multi-file mode — patch-based editing with file-level patches | 1A | patch.c — V4A patch mode, registry |
    12|| T24 | Todo tool depth — merge/write modes, in_progress/cancelled status, summary counts with breakdown, normalized items | 1A | todo.c — merge/write/actions, build_summary(), normalize_item(), in_progress+cancelled status enum |
    13|| T25 | Cronjob pause/resume/run actions — pause (disable w/o remove), resume (re-enable), run (trigger immediately via cron_run_job) | 1A | cronjob.c — pause/resume/run actions, schema updated; cron_sqlite.c — cron_sqlite_get_command() |
    14|| T26 | Binary detection — wired has_binary_extension() into file.c read handler via lib/libbinary/binary.c. Removed stale #41 binary_extensions from missing tools list. | 1B | file.c — #include binary.h, is_binary detection in handle_read(); lib/libbinary/binary.c + binary.h |
    15|| T27 | Edit approval tests — 44 comprehensive tests covering sensitive path detection, proposal building (write_file + patch), auto-approval policies (ask/session/workspace_session), and approval request lifecycle | 10 | tests/test_edit_approval.c — 44/44 pass, registered in test_runner.sh |
    16|| T18 | File syntax check — auto-detect type from extension, type override param, registered as file_syntax tool | 1A | file.c:826-927 (handle_syntax), 5 tests |
    17|| T19 | HomeAssistant full parity — domain+area filtering, entity_id+data+validation+blocked domains, ha_get_history | 1A | homeassistant.c: ha_call_service, ha_get_history |
    18|| T22 | Skill deps — depends_on resolution from frontmatter | 1A | skill_mgmt.c: deps action |
    19|
    20|## Phase 60: CI/Integration Stale Cleanup + Static Analysis — S22 (2026-05-25)
    21|
    22|| ID | Description | Sector | Evidence |
    23||----|-------------|--------|----------|
    24|| I01 | GitHub Actions CI for C build — already exists in c-build.yml (build + test + ASan + coverage + Docker + perf) | S22 (stale) | .github/workflows/c-build.yml — full CI pipeline with build, test-suite, ASan, coverage, Docker build, perf gate |
    25|| I02 | ASan in CI — already exists as asan job in c-build.yml | S22 (stale) | c-build.yml lines 87-120: ASan build + test under sanitizer |
    26|| I03 | Code coverage reporting — gcov/lcov integration exists | S22 (stale) | c-build.yml lines 185-223: coverage job with lcov HTML report |
    27|| I05 | Benchmark regression detection — already exists as perf job | S22 (stale) | c-build.yml lines 307-330: perf job runs make perf-gate |
    28|| I06 | Release workflow — c-release.yml publishes tagged releases | S22 (stale) | .github/workflows/c-release.yml — GitHub Release on v* tags |
    29|| I07 | Docker build for C image — C/Dockerfile exists, built in CI | S22 (stale) | C/Dockerfile (45 LOC multi-stage), c-build.yml lines 175-183: docker build test |
    30|| A32 | tool_dispatch_helpers — fully ported in lib/libtooldispatch/ (304 LOC) | S4 (stale) | lib/libtooldispatch/tool_dispatch_helpers.c — stateless dispatch utilities matching Python |
    31|
    32|## Phase 61: Static Analysis in CI — I04 (2026-05-25)
    33|
    34|| ID | Description | Sector | Evidence |
    35||----|-------------|--------|----------|
    36|| I04 | Added cppcheck static analysis to c-build.yml CI pipeline | S22 | .github/workflows/c-build.yml — new "Static analysis (cppcheck)" step |
    37|
    38|## Phase 62: Stale Claims Sweep + Vault Key Rotation (2026-05-25)
    39|
    40|### Stale Claims Retired
    41|
    42|| ID | Description | Sector | Evidence |
    43||----|-------------|--------|----------|
    44|| M01 | anthropic_adapter — thinking block → reasoning extraction, tool_use block parsing, tool format conversion all fully implemented in provider_anthropic.c | S5 (stale) | provider_anthropic.c |
    45|| M09 | model_metadata — 25+ model entries with context_window, MODEL_CAP flags | S5 (stale) | provider_metadata.c |
    46|| M12 | prompt_builder AGENTS.md/CLAUDE.md loading — system_prompt.c has full context file loading | S5 (stale) | system_prompt.c:513-590 |
    47|| M13 | stream_diag — Ttfb tracking, first_token_time, token_count, tokens_per_second | S5 (stale) | llm_client.c:1035-1038, 1058-1065, 1304-1314 |
    48|| G11 | slack rich formatting — Block Kit support | S8 (stale) | slack.c:71-95 |
    49|| G14 | discord slash command registration — full REST API | S8 (stale) | discord.c:212-255 |
    50|| G15 | telegram inline query mode — answerInlineQuery exists | S8 (stale) | telegram.c:303-315 |
    51|| G19 | bluebubbles iMessage attachment handling | S8 (stale) | bluebubbles.c:173-217 |
    52|
    53|### Gap Closed
    54|
    55|| ID | Description | LOC | Priority | Evidence |
    56||----|-------------|-----|----------|----------|
    57|| V01 | Key rotation — vault_rotate_key() with two modes, full rollback | 60 | P2 | vault.c:345-405, tests/test_vault.c:192-242 |
    58|
    59|## Phase 63: Display Parity Session — Phase 0b (2026-05-25)
    60|
    61|Closed 9 real gaps (V01-V09: skin engine, KawaiiSpinner, banner, status bar, tool feed, response box, help formatting, 256-color palette, prompt symbol). 2 stale claims removed.
    62|
    63|## Phase 59: Cloud Metadata Endpoint Detection — S01 (2026-05-25)
    64|
    65|| ID | Description | Sector | Evidence |
    66||----|-------------|--------|----------|
    67|| S01 | Added always-blocked cloud metadata endpoint patterns | S19 | url_safety.c |
    68|
    69|## Phase 58: Skill Slash-Command Injection — A28 (2026-05-25)
    70|
    71|| ID | Description | Sector | Evidence |
    72||----|-------------|--------|----------|
    73|| A28 | skill_commands.c — port Python skill_commands.py to C | S4 | skill_commands.c — 7 public API functions |
    74|
    75|## Phase 57: Mixture of Agents Tool — N02 (2026-05-25)
    76|
    77|| ID | Description | Sector | Evidence |
    78||----|-------------|--------|----------|
    79|| N02 | Mixture of Agents tool — queries 4 reference models via C LLM provider infra | S20 | src/tools/mixture_of_agents.c |
    80|
    81|## Phase 56: Feishu Doc/Drive Tools — D22 (2026-05-25)
    82|
    83|| ID | Description | Sector | Evidence |
    84||----|-------------|--------|----------|
    85|| D22 | Feishu doc/drive tool support (doc_read, drive_list) | S7 | src/tools/feishu_tools.c |
    86|
    87|## Phase 54: REST API Config/Service/Metrics — E01 (2026-05-25)
    88|
    89|| ID | Description | Sector | Evidence |
    90||----|-------------|--------|----------|
    91|| E01 | REST API endpoints — /v1/config, /v1/service/info, /v1/metrics | S13 | api_server.c |
    92|
    93|## Earlier Phases (1-8, 9-11)
    94|
    95|See prior vault contents for Phases 1-8 (Foundation, Agent Core, CLI & Commands, Tools, Gateway Platforms, Library Ports, Plugin System, Stale Claims Retired), Phases 9-11 (Battleship-v8 Stale Claims Retired, CLI Commands Depth, Agent Module Ports), and Phases 18-70 (per-gap closures).
    96|
    97|### cronjob update action (2026-05-27)
    98|- **Gap:** cronjob_tools update (edit job fields) — C had list/add/remove/config/pause/resume/run, missing `update` action
    99|- **Fix:** added `update` action handler to cronjob.c that modifies schedule, command, notify_on_complete, notify_on_failure, retry, backoff, and context_from via cron_sqlite_update_job API
   100|- **Evidence:** `src/tools/cronjob.c` — `update` action handler (~85 lines)
   101|- **Impact:** -1 gap (~373→~372), Phase 3 Tool Features 48→47
   102|
   103|### cronjob list action with real jobs & name filter (2026-05-27)
   104|- **Gap:** cron_list_jobs() returned `[]` stub in jobs.c. Also missing optional name filter in list action.
   105|- **Fix:** Added `cron_sqlite_list_to_json()` to cron_sqlite.c that serializes the job store to JSON. Rewired cronjob.c list action to use SQLite store instead of stub. Added optional `name` param for substring filtering.
   106|- **Evidence:** `src/cron/cron_sqlite.c` — `cron_sqlite_list_to_json()` function; `src/tools/cronjob.c` — list action with name filter
   107|- **Test fix:** Added `g_cron_store` + `cron_sqlite_list_to_json` stubs to test_cron_tool.c (25 cron tool tests pass)
   108|- **Impact:** Cron jobs now actually show up in /cron list. Named filter enables targeted lookup. 1 gap reduction in cronjob row (2→1).
   109|
   110|## Phase 13: file_hash Tool + Test Fix (2026-05-27)
   111|
   112|| ID | Description | Sector | Evidence |
   113||----|-------------|--------|----------|
   114|| T28 | file_hash tool — registered SHA-256/SHA-1/MD5 file hashing tool using existing hash_sha256_file/hash_sha1_hex/hash_md5 functions from libhash. Was fully implemented (handle_hash + SCHEMA_HASH) but never registered. | 1A | file.c: registry_register("file_hash", ...); handle_hash() + file_hash_handler() |
   115|| T29 | file_tool test compilation fix — added missing -I libbinary and binary.c to test_runner.sh compilation. The test was SKIP'd due to `#include "binary.h"` not found. Also changed tmpdir from /tmp/hermes_test_file (conflicted with binary path) to /tmp/hermes_test_file_data. Added 5 hash test assertions (58 total). | 10 | test_runner.sh: binary.c + -I libbinary; test_file.c: tmpdir path, extern file_hash_handler, hash test cases |
   116|| - | 78 tools (was 77). Suite 231/0/24 (was 230/0/25 — file_tool test now runs instead of skip). | | |
   117|
   118|## Phase 14: x_search enrichment (2026-05-27)
   119|
   120|| ID | Description | Sector | Evidence |
   121||----|-------------|--------|----------|
   122|| T30 | x_search: added enable_image_understanding + enable_video_understanding boolean params. C x_search was missing these 2 features that Python has. Added to both schema and tool_def construction. | 1A | x_search.c: SCHEMA + handler tool_def builder |
   123|| - | 78 tools unchanged. Suite unchanged. 2 schema params closer to Python parity. | | |
   124|
   125|## Phase 15: image_generate enrichment (2026-05-27)
   126|
   127|| ID | Description | Sector | Evidence |
   128||----|-------------|--------|----------|
   129|| T31 | image_generate: added negative_prompt + style params to schema + request body builder. C was missing these 2 params that Python supports via FAL's flux-pro API. | 1A | image_gen.c: SCHEMA + body building with negative_prompt/style |
   130|| - | 78 tools unchanged. Suite unchanged. | | |
   131|
   132|## Phase 16: web_get enrichment (2026-05-27)
   133|
   134|| ID | Description | Sector | Evidence |
   135||----|-------------|--------|----------|
   136|| T32 | web_get: added method/headers/body params. C web_get only supported GET with default Accept header. Now supports GET/POST/PUT/DELETE methods, custom headers string, and request body for POST/PUT. Schema updated with 3 new optional params. | 1A | web.c: updated SCHEMA_GET + handler with method_str_to_enum(), headers string passthrough, body_len passthrough to http_request() |
   137|| - | 78 tools unchanged. Suite 231/0/24. web_tools gap reduced 12→9. | |
   138|
   139|## Phase 17: video_gen model param (2026-05-27)
   140|
   141|| ID | Description | Sector | Evidence |
   142||----|-------------|--------|----------|
   143|| T33 | video_generate: added model param (model override string). C video_gen was missing the model override param that Python supports. Added to schema + body builder. | 1A | video_gen.c: SCHEMA + body building with model |
   144|| - | 78 tools unchanged. Suite 231/0/24. video_gen gap reduced 9→8. | |
   145|
   146|## Phase 18: cronjob timezone param (2026-05-27)
   147|
   148|| ID | Description | Sector | Evidence |
   149||----|-------------|--------|----------|
   150|| T34 | cronjob: added timezone param for per-job timezone. C cronjob was missing timezone support. Now stores/retrieves per-job timezone via cron_sqlite_update_job. Added to add/config/update actions. | 1A | cronjob.c: SCHEMA + handlers (add/config/update) |
   151|| - | 78 tools unchanged. Suite 231/0/24. timezone sub-gap closed. | |
   152|
   153|## Phase 19: video_gen reference_image_urls (2026-05-27)
   154|
   155|| ID | Description | Sector | Evidence |
   156||----|-------------|--------|----------|
   157|| T35 | video_generate: added reference_image_urls param (JSON array of URL strings). C video_gen was missing reference image support that Python provides. Added to schema + body builder using json_serialize for clean array embedding. | 1A | video_gen.c: SCHEMA + body building with json_serialize(ref_imgs) |
   158|| - | 78 tools unchanged. Suite 231/0/24. video_gen gap reduced 8→7. | |
   159|
   160|## Phase 20: web_search lang param (2026-05-27)
   161|
   162|| ID | Description | Sector | Evidence |
   163||----|-------------|--------|----------|
   164|| T36 | web_search: added lang filter param. C web_search was missing language filtering. Added to schema + search_searxng URL builder. Supports searxng backend (lang query param). | 1A | web.c: SCHEMA_SEARCH + search_searxng URL builder + handler extraction |
   165|| - | 78 tools unchanged. Suite 231/0/24. | |
   166|
   167|## Phase 21: web_get proxy param (2026-05-27)
   168|
   169|| ID | Description | Sector | Evidence |
   170||----|-------------|--------|----------|
   171|| T37 | web_get: added proxy param for HTTP proxy support. C web_get was missing proxy auth support. Added to schema + handler using http_client_set_proxy API. Supports HTTP proxy URL (CONNECT tunnel for HTTPS). | 1A | web.c: SCHEMA_GET + http_client_set_proxy(client, proxy_copy) |
   172|| - | 78 tools unchanged. Suite 231/0/24. web_tools gap reduced 9→8. | |
   173|
   174|## Phase 22: web_extract format param (2026-05-27)
   175|
   176|| ID | Description | Sector | Evidence |
   177||----|-------------|--------|----------|
   178|| T38 | web_extract: added format param (markdown/html). C web_extract was missing output format selection. Added to schema + delegate input. | 1A | web.c: SCHEMA_EXTRACT + handler extraction + delegate input |
   179|| - | 78 tools unchanged. Suite 231/0/24. | |
   180|
   181|## Phase 23: send_message thread_id param (2026-05-27)
   182|
   183|| ID | Description | Sector | Evidence |
   184||----|-------------|--------|----------|
   185|| T39 | send_message: added thread_id param for threaded conversations. C send_message was missing thread/topic support. Added to schema + extraction + result output. | 1A | send_message.c: SCHEMA + handler extraction + result |
   186|| - | 78 tools unchanged. Suite 231/0/24. | |
   187|
   188|## Phase 24: web_get user_agent param (2026-05-27)
   189|
   190|| ID | Description | Sector | Evidence |
   191||----|-------------|--------|----------|
   192|| T40 | web_get: added user_agent param for custom User-Agent header. C web_get used default User-Agent. Now supports custom UA. Prepended to headers string when set. | 1A | web.c: SCHEMA_GET + handler with User-Agent header building |
   193|| - | 78 tools unchanged. Suite 231/0/24. | |
   194|
   195|## Phase 25: vision_analyze detail param (2026-05-27)
   196|
   197|| ID | Description | Sector | Evidence |
   198||----|-------------|--------|----------|
   199|| T41 | vision_analyze: added detail param (low/high/auto). C vision was missing detail level control. Added to schema + extraction + delegate script args. | 1A | vision.c: SCHEMA + handler extraction + delegate args |
   200|| - | 78 tools unchanged. Suite 231/0/24. | |
   201|
   202|## Phase 26: x_search media_filter param (2026-05-27)
   203|
   204|| ID | Description | Sector | Evidence |
   205||----|-------------|--------|----------|
   206|| T42 | x_search: added media_filter param (images/videos/news/links). C x_search was missing media type filtering. Added to schema + tool_def builder. | 1A | x_search.c: SCHEMA + tool_def builder with media_filter |
   207|| - | 78 tools unchanged. Suite 231/0/24. | |
   208|
   209|## Phase 27: x_search max_results param (2026-05-27)
   210|
   211|| ID | Description | Sector | Evidence |
   212||----|-------------|--------|----------|
   213|| T43 | x_search: added max_results param (1-50, default 10). C x_search was missing result count control. Added to schema + tool_def builder. | 1A | x_search.c: SCHEMA + tool_def with max_results |
   214|| - | 78 tools unchanged. Suite 231/0/24. | |
   215|
   216|## Phase 28: transcribe language param (2026-05-27)
   217|
   218|| ID | Description | Sector | Evidence |
   219||----|-------------|--------|----------|
   220|| T44 | transcribe: added language param for language hint. C transcribe was missing spoken language hint. Added to schema + handler extraction. | 1A | transcribe.c: SCHEMA + handler extraction |
   221|| - | 78 tools unchanged. Suite 231/0/24. | |
   222|
   223|## Phase 29: tts speed param (2026-05-27)
   224|
   225|| ID | Description | Sector | Evidence |
   226||----|-------------|--------|----------|
   227|| T45 | tts: added speed param (0.5-2.0). C TTS was missing speech speed control. Added to schema + extraction + espeak-ng -s flag + result output. | 1A | tts.c: SCHEMA + handler with espeak-ng speed integration |
   228|| - | 78 tools unchanged. Suite 231/0/24. | |
   229|| | **Phase 10 (2026-05-27)** | | |
   230|| | - | web_get: follow_redirects + max_redirects params. Phase 3: 26→25. | src/tools/web.c | web_get schema + handler |
   231|| | - | x_search: added lang param (ISO 639-1). Phase 3: 25→24. | src/tools/x_search.c | x_search schema + handler |
   232||| - | skill_manage: added pin + unpin actions (freeze/guard). Phase 3: 24→22. | src/tools/skill_mgmt.c | pin guard, action_pin, action_unpin, dispatch |
   233||| - | **Phase 11 (2026-05-27)** | | |
   234||| - | web_get: added cookies param (Cookie header support). Phase 3: 22→21. | src/tools/web.c | web_get schema + handler + Cookie header injection |
   235||
|| - | file_merge: added test (4 error-path tests). Suite: 229->230. Made handler non-static for testability. | tests/test_file_merge.c test_runner.sh | 4 tests, test_stubs.c infrastructure |

## Phase 30: OSV malware check wiring (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| U01 | osv_check: wired lib/libosv into mcp_tool connect_stdio_server. osv.c was fully implemented (282 LOC, 3 public functions) but never called. Now checks npx/uvx packages against OSV API for MAL-* advisories before connecting. Also added arg_count param to connect_stdio_server signature. | 1A (unwired stub) | src/tools/mcp_tool.c — #include "osv.h", osv_check_package_for_malware() in connect_stdio_server path, skip_server goto for cleanup |
   236|

## Phase 13: Battleship-v16 1B Stale Claim Sweep (2026-05-27)

All 26 "missing tools" in the battleship's 1B section were verified as already implemented in C. Moved to vault to clean up active gap list.

| ID | Old Claim | Sector | Actual Location | Evidence |
|----|-----------|--------|-----------------|----------|
| 1B-stale-01 | skills_hub (192 fns, ~3500 LOC) — "missing" | 1B | src/skills_hub.c | Full implementation |
| 1B-stale-02 | checkpoint_manager (39 fns, ~1000 LOC) — "missing" | 1B | src/agent/checkpoint.c | 253 LOC, init/save/rollback |
| 1B-stale-03 | process_registry (38 fns, ~800 LOC) — "missing" | 1B | src/tools/process.c | 559 LOC, full lifecycle |
| 1B-stale-04 | mcp_oauth (34 fns, ~900 LOC) — "missing" | 1B | src/tools/mcp_tool.c | OAuth flow in mcp_tool |
| 1B-stale-05 | clarify_gateway (13 fns, ~300 LOC) — "missing" | 1B | src/tools/clarify.c | Full gateway clarify |
| 1B-stale-06 | website_policy (11 fns, ~250 LOC) — "missing" | 1B | lib/libwebsite/website_policy.c | Robots.txt parser |
| 1B-stale-07 | tool_result_storage (7 fns, ~200 LOC) — "missing" | 1B | src/tools/result_storage.c | Result caching |
| 1B-stale-08 | threat_patterns (3 fns, ~300 LOC) — "missing" | 1B | src/tools/tirith.c | Security patterns |
| 1B-stale-09 | tool_output_limits (5 fns, ~200 LOC) — "missing" | 1B | src/tools/tool_result.c | Output limits |
| 1B-stale-10 | budget_config (2 fns, ~150 LOC) — "missing" | 1B | src/agent/budget_tracker.c | 313 LOC budget tracking |
| 1B-stale-11 | debug_helpers (7 fns, ~150 LOC) — "missing" | 1B | lib/libdebug/debug_helpers.c | Debug logging helpers |
| 1B-stale-12 | slash_confirm (6 fns, ~100 LOC) — "missing" | 1B | src/tools/approval.c | Confirmation dialogs |
| 1B-stale-13 | tool_backend_helpers (9 fns, ~200 LOC) — "missing" | 1B | lib/libtoolbackend/tool_backend.c | Backend dispatch |
| 1B-stale-14 | managed_tool_gateway (10 fns, ~300 LOC) — "missing" | 1B | lib/libmangateway/managed_gateway.c | Gateway exec |
| 1B-stale-15 | env_passthrough (7 fns, ~100 LOC) — "missing" | 1B | lib/libenvpassthrough/env_passthrough.c | Env passthrough |
| 1B-stale-16 | file_state (20 fns, ~400 LOC) — "missing" | 1B | lib/libfilestate/file_state.c | File state tracking |
| 1B-stale-17 | fuzzy_match (28 fns, ~600 LOC) — "missing" | 1B | lib/libfuzzymatch/fuzzy_match.c | Fuzzy matching |
| 1B-stale-18 | schema_sanitizer (9 fns, ~200 LOC) — "missing" | 1B | lib/libschemasanitizer/schema_sanitizer.c | Schema sanitization |
| 1B-stale-19 | skills_sync (9 fns, ~400 LOC) — "missing" | 1B | lib/libskillsync/skills_sync.c | Skill sync |
| 1B-stale-20 | skill_provenance (4 fns, ~200 LOC) — "missing" | 1B | lib/libskillusage/skill_provenance.c | Provenance tracking |
| 1B-stale-21 | fallback_resolver (2 fns, ~100 LOC) — "missing" | 1B | src/agent/fallback_routing.c | Credential resolution |
| 1B-stale-22 | xai_http (4 fns, ~200 LOC) — "missing" | 1B | lib/libxai_http/xai_http.c | xAI HTTP client |
| 1B-stale-23 | osv_check (6 fns, ~200 LOC) — "missing" | 1B | lib/libosv/osv.c | OSV vuln DB |
| 1B-stale-24 | interrupt (7 fns, ~200 LOC) — "missing" | 1B | lib/libinterrupt/interrupt.c | Cross-platform interrupt |
| 1B-stale-25 | ansi_strip (1 fn, ~100 LOC) — "missing" | 1B | src/tools/ansi_strip.c | ANSI stripping |
| 1B-stale-26 | path_security (2 fns, ~200 LOC) — "missing" | 1B | src/tools/path_security.c | Path traversal detection |

Truly missing tools from 1B section: skills_guard, credential_files, skills_ast_audit, neutts_synth, microsoft_graph_client, microsoft_graph_auth, mcp_oauth_manager, lazy_deps.

## Phase 14: Batch chmod — file_batch tool depth (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| F15-01 | Batch chmod action — parse octal mode string, per-file sandbox check, apply chmod via POSIX chmod(), per-file result reporting with success/fail counts | 1A (file_operations) | src/tools/file_batch.c — parse_mode_string(), chmod_file(), chmod branch in file_batch_handler() |
| F15-02 | Batch touch action — update timestamps via utimensat() or create empty file via fopen(). Per-file sandbox check and result reporting | 1A (file_operations) | src/tools/file_batch.c — touch_file(), touch branch in file_batch_handler() |
| F15-03 | Batch stat action — stat each file, returns JSON with size/mode/mtime/is_dir. Per-file sandbox check. | 1A (file_operations) | src/tools/file_batch.c — stat_file_json(), stat branch in handler |
| F15-04 | Batch hash action — SHA-256 via libhash hash_sha256_hex(). Reads file (100MB cap), computes hash, returns sha256 hex string. Per-file sandbox. | 1A (file_operations) | src/tools/file_batch.c — hash_file_json(), hash branch in handler |
| TERM-01 | Fixed JSON schema bug in terminal.c: extra escaped quote after `modal` in backend description. Schema fragment `...modal\"}\",\"` produced invalid JSON — stray `"` between `}` and `,`. Lenient parsers tolerated it; strict validation would reject | 1A (terminal_tool) | src/tools/terminal.c line 36 — corrected `}\",\"` to `},\"` |

## Phase 15: Batch rename — file_batch tool depth (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| F15-05 | Batch rename action — POSIX glob-based pattern matching, wildcard extraction/substitution. Supports `pattern`+`dest_pattern` (e.g. `*.txt` → `backup_*.md`) and simple files+dest rename. Per-file sandbox check. | 1A (file_operations) | src/tools/file_batch.c — extract_wildcard(), substitute_wildcard(), apply_rename_pattern(), early-return rename-with-pattern block, simple rename branch in loop |

## Phase 15: Batch convert — file_batch tool depth (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| F15-06 | Batch convert — case conversion (upper/lower/title), line ending conversion (lf/crlf/cr), ASCII encoding strip. Reads file, transforms in-place, writes only if changed. 100MB cap. Per-file sandbox check. | 1A (file_operations) | src/tools/file_batch.c — convert_file() handler, convert branch in loop |

## Phase 16: Permission validation + dry-run — file_batch (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| F15-07 | Permission validation via `access()` — read check (R_OK) for stat/hash, write check (W_OK) for copy/move/delete/chmod/touch/convert/rename. Also checks parent dir writability for dest paths. | 1A (file_operations) | src/tools/file_batch.c — check_file_access(), check_parent_writable(), added per-action permission checks |
| F15-08 | Dry-run mode — `dry_run` bool param. When true, reports what would happen (would_copy, would_move, would_delete, would_chmod, would_touch, would_convert, would_rename) without modifying any files. Includes dest path in response for applicable actions. | 1A (file_operations) | src/tools/file_batch.c — dry_run param in schema, `if (dry_run)` branches in every action in both loop and early-return rename block |

## Phase 17: Geo location filter — x_search (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| XS-01 | Geo location filter for X search — added geo_lat, geo_long, geo_radius_km params. Builds geo JSON object with lat/long/radius_km and passes to xAI API tool definition. Default radius 25km. | 1A (x_search_tool) | src/tools/x_search.c — SCHEMA + handler geo block after lang |

## Phase 18: User search mode — x_search (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| XS-02 | User search mode — added `search_type` param ("posts" or "users"). When "users", sets tool type to x_user_search and sets query directly. Handles, dates, media, geo filters skipped for user search since API doesn't support them. | 1A (x_search_tool) | src/tools/x_search.c — search_type parsing, conditional tool type in tool_def builder, else block closure |

## Phase 19: Seed + num_images params — image_gen (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| IG-01 | Seed parameter — added `seed` (int, optional, 0=random) for reproducible image generation. When >0, passed as `seed` field to FAL API | 1A (image_generation_tool) | src/tools/image_gen.c — seed param extraction at line 34, body construction at line 66 |
| IG-02 | Num images parameter — added `num_images` (int, optional, 1-4, default 1) for generating multiple images per prompt. Capped at 4. | 1A (image_generation_tool) | src/tools/image_gen.c — num_images param extraction at line 35, cap at 4, body construction at line 70 |

## Phase 20: reply_to_message_id — send_message tool (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| SM-01 | reply_to_message_id param — added `reply_to_message_id` (string, optional, platform-specific format) for replying to specific messages. Wired into telegram sendMessage as `--reply_to` flag and generic platform routing. | 1A (send_message_tool) | src/tools/send_message.c — schema param at line 21, handler extraction at line 38, telegram text send at line 174, generic platform send at line 217 |

## Phase 21: Color analysis + EXIF extraction — vision tool (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| VI-01 | Color analysis — added `analysis="colors"` param. Extracts 8 dominant colors via Python PIL (Counter + getdata), returns hex codes. | 1A (vision_tools) | src/tools/vision.c — analysis param handling, run_cmd_full to vision_analysis.py colors |
| VI-02 | EXIF extraction — added `analysis="exif"` param. Reads EXIF tags via Python PIL (ExifTags), returns key:value pairs. | 1A (vision_tools) | src/tools/vision.c — analysis param handling, run_cmd_full to vision_analysis.py exif |

## Phase 22: Image-to-image (image_url param) — image_gen (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| IG-03 | Image-to-image generation — added `image_url` (string, optional) param. When provided, passes as `image_url` field in FAL API request body for img2img generation. | 1A (image_generation_tool) | src/tools/image_gen.c — image_url param extraction, body construction included when present, schema entry with description |
| VI-03 | Vision analysis helper script — separate Python script for PIL-based image analysis (colors + EXIF). | 1A (vision_tools) | src/tools/vision_analysis.py — analyze_colors(), analyze_exif() functions |

## Phase 23: SSRF protection — web tool (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| WT-01 | SSRF protection — added url_is_safe() check before each HTTP request. Blocks private/internal IP addresses (loopback, RFC1918, link-local, etc.). Added hermes_url_safety.h include and url_safety.c to test compilation. | 1A (web_tools) | src/tools/web.c — SSRF check after URL validation, test_runner.sh — url_safety.c linked in web depth test |

## Phase 24: output_format param — image_gen (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| IG-04 | Output format parameter — added `output_format` (string, optional) param for controlling image output format (png, jpeg, webp). Passed as output_format field in FAL API request. | 1A (image_generation_tool) | src/tools/image_gen.c — output_format param extraction, body construction, schema entry |

## Phase 25: save_local param — image_gen (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| IG-05 | Save local toggle — added `save_local` (boolean, default true) param. When false, skips downloading/ writing the generated image to /tmp. Returns URL only for faster response. | 1A (image_generation_tool) | src/tools/image_gen.c — save_local param extraction, conditional download block, schema entry |

## Phase 26: sort_order param — x_search (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| XS-03 | Sort order search param — added `sort_order` (string, "relevance" or "recency", default "relevance") param for controlling X search result ordering. | 1A (x_search_tool) | src/tools/x_search.c — sort_order schema entry, handler wiring in tool_def builder |

## Phase 27: exclude_retweets param — x_search (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| XS-04 | Exclude retweets — added `exclude_retweets` (boolean, default false) param. When true, sets exclude_retweets=true in xAI tool definition. | 1A (x_search_tool) | src/tools/x_search.c — exclude_retweets schema entry, handler wiring |
