# Slermes C Translation — Vault of Achievements

All completed work archived here. Clears the active gap list for fresh battleship generation.
Last updated: 2026-05-26

## Phase 12: Battleship-v15 Resolved Items (2026-05-26)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T21 | Approval gateway prompt — gateway callback pattern, Telegram short-poll for async response, cross-platform condvar | 1A | approval.c + server.c — approval_set_gateway_send/wait, gw_approval_set_context, gw_approval_wait_response |
| T23 | Patch V4A multi-file mode — patch-based editing with file-level patches | 1A | patch.c — V4A patch mode, registry |
| T24 | Todo tool depth — merge/write modes, in_progress/cancelled status, summary counts with breakdown, normalized items | 1A | todo.c — merge/write/actions, build_summary(), normalize_item(), in_progress+cancelled status enum |
| T25 | Cronjob pause/resume/run actions — pause (disable w/o remove), resume (re-enable), run (trigger immediately via cron_run_job) | 1A | cronjob.c — pause/resume/run actions, schema updated; cron_sqlite.c — cron_sqlite_get_command() |
| T26 | Binary detection — wired has_binary_extension() into file.c read handler via lib/libbinary/binary.c. Removed stale #41 binary_extensions from missing tools list. | 1B | file.c — #include binary.h, is_binary detection in handle_read(); lib/libbinary/binary.c + binary.h |
| T27 | Edit approval tests — 44 comprehensive tests covering sensitive path detection, proposal building (write_file + patch), auto-approval policies (ask/session/workspace_session), and approval request lifecycle | 10 | tests/test_edit_approval.c — 44/44 pass, registered in test_runner.sh |
| T18 | File syntax check — auto-detect type from extension, type override param, registered as file_syntax tool | 1A | file.c:826-927 (handle_syntax), 5 tests |
| T19 | HomeAssistant full parity — domain+area filtering, entity_id+data+validation+blocked domains, ha_get_history | 1A | homeassistant.c: ha_call_service, ha_get_history |
| T22 | Skill deps — depends_on resolution from frontmatter | 1A | skill_mgmt.c: deps action |

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
| I04 | Added cppcheck static analysis to c-build.yml CI pipeline | S22 | .github/workflows/c-build.yml — new "Static analysis (cppcheck)" step |

## Phase 62: Stale Claims Sweep + Vault Key Rotation (2026-05-25)

### Stale Claims Retired

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| M01 | anthropic_adapter — thinking block → reasoning extraction, tool_use block parsing, tool format conversion all fully implemented in provider_anthropic.c | S5 (stale) | provider_anthropic.c |
| M09 | model_metadata — 25+ model entries with context_window, MODEL_CAP flags | S5 (stale) | provider_metadata.c |
| M12 | prompt_builder AGENTS.md/CLAUDE.md loading — system_prompt.c has full context file loading | S5 (stale) | system_prompt.c:513-590 |
| M13 | stream_diag — Ttfb tracking, first_token_time, token_count, tokens_per_second | S5 (stale) | llm_client.c:1035-1038, 1058-1065, 1304-1314 |
| G11 | slack rich formatting — Block Kit support | S8 (stale) | slack.c:71-95 |
| G14 | discord slash command registration — full REST API | S8 (stale) | discord.c:212-255 |
| G15 | telegram inline query mode — answerInlineQuery exists | S8 (stale) | telegram.c:303-315 |
| G19 | bluebubbles iMessage attachment handling | S8 (stale) | bluebubbles.c:173-217 |

### Gap Closed

| ID | Description | LOC | Priority | Evidence |
|----|-------------|-----|----------|----------|
| V01 | Key rotation — vault_rotate_key() with two modes, full rollback | 60 | P2 | vault.c:345-405, tests/test_vault.c:192-242 |

## Phase 63: Display Parity Session — Phase 0b (2026-05-25)

Closed 9 real gaps (V01-V09: skin engine, KawaiiSpinner, banner, status bar, tool feed, response box, help formatting, 256-color palette, prompt symbol). 2 stale claims removed.

## Phase 59: Cloud Metadata Endpoint Detection — S01 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| S01 | Added always-blocked cloud metadata endpoint patterns | S19 | url_safety.c |

## Phase 58: Skill Slash-Command Injection — A28 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| A28 | skill_commands.c — port Python skill_commands.py to C | S4 | skill_commands.c — 7 public API functions |

## Phase 57: Mixture of Agents Tool — N02 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| N02 | Mixture of Agents tool — queries 4 reference models via C LLM provider infra | S20 | src/tools/mixture_of_agents.c |

## Phase 56: Feishu Doc/Drive Tools — D22 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| D22 | Feishu doc/drive tool support (doc_read, drive_list) | S7 | src/tools/feishu_tools.c |

## Phase 54: REST API Config/Service/Metrics — E01 (2026-05-25)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| E01 | REST API endpoints — /v1/config, /v1/service/info, /v1/metrics | S13 | api_server.c |

## Earlier Phases (1-8, 9-11)

See prior vault contents for Phases 1-8 (Foundation, Agent Core, CLI & Commands, Tools, Gateway Platforms, Library Ports, Plugin System, Stale Claims Retired), Phases 9-11 (Battleship-v8 Stale Claims Retired, CLI Commands Depth, Agent Module Ports), and Phases 18-70 (per-gap closures).

### cronjob update action (2026-05-27)
- **Gap:** cronjob_tools update (edit job fields) — C had list/add/remove/config/pause/resume/run, missing `update` action
- **Fix:** added `update` action handler to cronjob.c that modifies schedule, command, notify_on_complete, notify_on_failure, retry, backoff, and context_from via cron_sqlite_update_job API
- **Evidence:** `src/tools/cronjob.c` — `update` action handler (~85 lines)
- **Impact:** -1 gap (~373→~372), Phase 3 Tool Features 48→47

### cronjob list action with real jobs & name filter (2026-05-27)
- **Gap:** cron_list_jobs() returned `[]` stub in jobs.c. Also missing optional name filter in list action.
- **Fix:** Added `cron_sqlite_list_to_json()` to cron_sqlite.c that serializes the job store to JSON. Rewired cronjob.c list action to use SQLite store instead of stub. Added optional `name` param for substring filtering.
- **Evidence:** `src/cron/cron_sqlite.c` — `cron_sqlite_list_to_json()` function; `src/tools/cronjob.c` — list action with name filter
- **Test fix:** Added `g_cron_store` + `cron_sqlite_list_to_json` stubs to test_cron_tool.c (25 cron tool tests pass)
- **Impact:** Cron jobs now actually show up in /cron list. Named filter enables targeted lookup. 1 gap reduction in cronjob row (2→1).

## Phase 13: file_hash Tool + Test Fix (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T28 | file_hash tool — registered SHA-256/SHA-1/MD5 file hashing tool using existing hash_sha256_file/hash_sha1_hex/hash_md5 functions from libhash. Was fully implemented (handle_hash + SCHEMA_HASH) but never registered. | 1A | file.c: registry_register("file_hash", ...); handle_hash() + file_hash_handler() |
| T29 | file_tool test compilation fix — added missing -I libbinary and binary.c to test_runner.sh compilation. The test was SKIP'd due to `#include "binary.h"` not found. Also changed tmpdir from /tmp/hermes_test_file (conflicted with binary path) to /tmp/hermes_test_file_data. Added 5 hash test assertions (58 total). | 10 | test_runner.sh: binary.c + -I libbinary; test_file.c: tmpdir path, extern file_hash_handler, hash test cases |
| - | 78 tools (was 77). Suite 231/0/24 (was 230/0/25 — file_tool test now runs instead of skip). | | |

## Phase 14: x_search enrichment (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T30 | x_search: added enable_image_understanding + enable_video_understanding boolean params. C x_search was missing these 2 features that Python has. Added to both schema and tool_def construction. | 1A | x_search.c: SCHEMA + handler tool_def builder |
| - | 78 tools unchanged. Suite unchanged. 2 schema params closer to Python parity. | | |

## Phase 15: image_generate enrichment (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T31 | image_generate: added negative_prompt + style params to schema + request body builder. C was missing these 2 params that Python supports via FAL's flux-pro API. | 1A | image_gen.c: SCHEMA + body building with negative_prompt/style |
| - | 78 tools unchanged. Suite unchanged. | | |

## Phase 16: web_get enrichment (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T32 | web_get: added method/headers/body params. C web_get only supported GET with default Accept header. Now supports GET/POST/PUT/DELETE methods, custom headers string, and request body for POST/PUT. Schema updated with 3 new optional params. | 1A | web.c: updated SCHEMA_GET + handler with method_str_to_enum(), headers string passthrough, body_len passthrough to http_request() |
| - | 78 tools unchanged. Suite 231/0/24. web_tools gap reduced 12→9. | |

## Phase 17: video_gen model param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T33 | video_generate: added model param (model override string). C video_gen was missing the model override param that Python supports. Added to schema + body builder. | 1A | video_gen.c: SCHEMA + body building with model |
| - | 78 tools unchanged. Suite 231/0/24. video_gen gap reduced 9→8. | |

## Phase 18: cronjob timezone param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T34 | cronjob: added timezone param for per-job timezone. C cronjob was missing timezone support. Now stores/retrieves per-job timezone via cron_sqlite_update_job. Added to add/config/update actions. | 1A | cronjob.c: SCHEMA + handlers (add/config/update) |
| - | 78 tools unchanged. Suite 231/0/24. timezone sub-gap closed. | |

## Phase 19: video_gen reference_image_urls (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T35 | video_generate: added reference_image_urls param (JSON array of URL strings). C video_gen was missing reference image support that Python provides. Added to schema + body builder using json_serialize for clean array embedding. | 1A | video_gen.c: SCHEMA + body building with json_serialize(ref_imgs) |
| - | 78 tools unchanged. Suite 231/0/24. video_gen gap reduced 8→7. | |

## Phase 20: web_search lang param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T36 | web_search: added lang filter param. C web_search was missing language filtering. Added to schema + search_searxng URL builder. Supports searxng backend (lang query param). | 1A | web.c: SCHEMA_SEARCH + search_searxng URL builder + handler extraction |
| - | 78 tools unchanged. Suite 231/0/24. | |

## Phase 21: web_get proxy param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T37 | web_get: added proxy param for HTTP proxy support. C web_get was missing proxy auth support. Added to schema + handler using http_client_set_proxy API. Supports HTTP proxy URL (CONNECT tunnel for HTTPS). | 1A | web.c: SCHEMA_GET + http_client_set_proxy(client, proxy_copy) |
| - | 78 tools unchanged. Suite 231/0/24. web_tools gap reduced 9→8. | |

## Phase 22: web_extract format param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T38 | web_extract: added format param (markdown/html). C web_extract was missing output format selection. Added to schema + delegate input. | 1A | web.c: SCHEMA_EXTRACT + handler extraction + delegate input |
| - | 78 tools unchanged. Suite 231/0/24. | |

## Phase 23: send_message thread_id param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T39 | send_message: added thread_id param for threaded conversations. C send_message was missing thread/topic support. Added to schema + extraction + result output. | 1A | send_message.c: SCHEMA + handler extraction + result |
| - | 78 tools unchanged. Suite 231/0/24. | |

## Phase 24: web_get user_agent param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T40 | web_get: added user_agent param for custom User-Agent header. C web_get used default User-Agent. Now supports custom UA. Prepended to headers string when set. | 1A | web.c: SCHEMA_GET + handler with User-Agent header building |
| - | 78 tools unchanged. Suite 231/0/24. | |

## Phase 25: vision_analyze detail param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T41 | vision_analyze: added detail param (low/high/auto). C vision was missing detail level control. Added to schema + extraction + delegate script args. | 1A | vision.c: SCHEMA + handler extraction + delegate args |
| - | 78 tools unchanged. Suite 231/0/24. | |

## Phase 26: x_search media_filter param (2026-05-27)

| ID | Description | Sector | Evidence |
|----|-------------|--------|----------|
| T42 | x_search: added media_filter param (images/videos/news/links). C x_search was missing media type filtering. Added to schema + tool_def builder. | 1A | x_search.c: SCHEMA + tool_def builder with media_filter |
| - | 78 tools unchanged. Suite 231/0/24. | |
