# State — Hermes C Translation (2026-05-26)

## ~55% toward 1:1 Python parity (~370 gaps remaining)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 2 depth | 98% | 322/322 YAML keys, profiles, `${VAR}`, `!include` |
| **Providers** | 35 | 87% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 16 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve |
| **Plugins** | 48 | 14% | 3 .so plugins: kanban + honcho + spotify (all real, 90 tests) |
| **Gateway** | 63 | **100% ✅** | All 63 gaps closed |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 31 | 86% | 23 state fields, 18 session DB, G01-G36 all filled |
| **CLI** | 33 | 87% | 70 slash commands, skin/theme engine. H14 --json, H31-H32 |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
| **Tests** | 37 | 60% | **60 files, 2,574+ assertions** (96 pass, 0 fail, 0 skip) |
| **Upstream** | 1 | new | L02 remains (CDP auto-launch, blocked) (125 commits behind) |
| **Cross-cut** | 4 | **100% (6/6) ✅** | Token counting, secure parent dir, key leakage, vendor key derivation, local trust |
| **Build/doc** | 9 | 60% | Dockerfile, CI, cross-compile, .dockerignore, man page, CHANGELOG |
| **Error types** | 0 | **50% ✅** | K01-K05: ValueError, TypeError, RuntimeError, OSError, TimeoutError |

**Known bug:** temperature=0.0 — **FIXED ✅**

### Session 2026-05-26 — CHANGELOG.md (O10)

- ✅ **CHANGELOG.md** — Full changelog from v0.1.0 (initial scaffold) through v0.7.0 (test/build infrastructure) to unreleased (current provider test coverage). 150 lines covering all 4 phases, 9 major releases, organized by feature area with suite stats per version.
- ◀ **Build/doc: 60%** (was 55%, O10 closed)
- ◀ Committed: `ee65389a3`

### Session 2026-05-26 — Azure provider full tests + 3 bugfixes

- ✅ **test_azure_full.c** — 45 assertions covering URL (deployment_id, api_version, trailing slash), headers, 12 LLM params, response_format/json_mode/strict, metadata, tool_choice, parallel_tool_calls, max_tool_calls, n, extra_body, messages (system/user/assistant/tool), tools JSON, response parsing (text/tool_calls/error), streaming (text delta/[DONE]/finish_reason/non-data prefix), null safety
- ✅ **Bugfix: 2x UAF (metadata + tool_choice)** — json_object_set then json_free
- ✅ **Bugfix: Error object not parsed** — added Azure API error handling in parse_response
- ✅ **Bugfix: NULL crash in azure_parse_stream_chunk** — strncmp on NULL
- ◀ **Suite: 96/0/0** (+1 test, 45 assertions; M05 closed)
- ◀ **Tests: 60%** (was 59%, 1/37 gaps closed)
- ◀ Committed: `d38e20f4e`

### Session 2026-05-26 — Bedrock provider full tests + 3 bugfixes

- ✅ **test_bedrock_full.c** — 35 assertions covering:
  - URL building (default region us-east-1, custom model)
  - Headers without AWS creds
  - Request body: inferenceConfig, system array, messages, B39-B41
  - response_format, json_mode, tool_choice, metadata
  - Response parsing: text, toolUse (arguments serialized), empty, error
  - Streaming chunk passthrough, null safety
- ✅ **Bugfix: UAF in metadata path** — `json_object_set` then `json_free` on same node
- ✅ **Bugfix: UAF in tool_choice parsing** — same pattern
- ✅ **Bugfix: toolUse input object deserialized as string** — `json_get_str` returned `{}` for object. Changed to `json_serialize`.
- ◀ **Suite: 95/0/0** (+1 test, 35 assertions; was 94/0/0)
- ◀ **Tests: 59%** (2 gaps closed: M03-M04)
- ◀ Committed: `b0a6c38b5`, `ef14d000e`, `6f0a44091`

### Session 2026-05-26 — Anthropic provider depth tests (B26-B28)

- ✅ **test_anthropic_depth.c** — 80 assertions covering:
  - URL building (default, custom, trailing slash, /messages suffix)
  - Headers (with/without API key, x-api-key format vs Bearer)
  - Request body: model, max_tokens, temperature, top_p, stop sequences
  - **B26: Thinking blocks** — reasoning_effort low/medium/high/direct number, budget_tokens mapping
  - **B27: Cache control** — last user message content block + system prompt on first call, plain string on cached
  - System message extraction to top-level "system" field, concatenation of multiple system messages
  - Tool format conversion: OpenAI's `{\\"type\\":\\"function\\",\\"function\\":{...}}` → Anthropic's `{\\"name\\":\\"...\\",\\"input_schema\\":{...}}`
  - Response parsing: text, thinking/reasoning, tool_use, error objects, usage tracking
  - Streaming: message_start, content_block_delta (text_delta + thinking_delta), content_block_start, message_delta (stop_reason), direct data: format
  - response_format, json_mode, tool_choice, parallel_tool_calls, extra_body merging
  - Null safety: NULL chunk, NULL response body, empty content blocks
- ✅ **Bugfix: NULL crash in anthropic_parse_stream_chunk** — added null safety before strncmp dereference
- ◀ **Suite: 93/0/0** (+1 test, 80 assertions; was 92/0/0)
- ◀ **Tests: 57%** (was 56%, 1/40 test gaps closed)
- ◀ **Providers: 87%** (B26-B28 coverage verified)
- ◀ Committed: `b0a6c38b5`

### Session 2026-05-26 — Google provider full tests + 3 bugfixes

- ✅ **test_google_full.c** — 40 assertions covering:
  - URL building (default, custom, trailing slash fix, :generateContent)
  - Headers with/without API key
  - Generation config: maxOutputTokens, temperature, topP, stopSequences, topK, candidateCount
  - B29: safety_settings JSON array parsing
  - System instruction extraction to systemInstruction with parts array
  - OpenAI→Google tool format conversion: functionDeclarations wrapper fix
  - response_format, json_mode, tool_choice, parallel_tool_calls, extra_body
  - Response parsing: text, functionCall, empty candidates, error
  - Streaming: text delta, finishReason, [DONE] marker, non-data prefix
  - Null safety: free_response, NULL/empty/invalid
- ✅ **Bugfix: Google tools functionDeclarations set on array** — `json_set(tools_arr, "functionDeclarations", ...)` on array was a no-op. Added wrapper object + append.
- ✅ **Bugfix: Trailing slash → //models** — stripped trailing slash in URL builder.
- ✅ **Bugfix: NULL crash in google_parse_stream_chunk** — strncmp on NULL.
- ◀ **Suite: 94/0/0** (+1 test, 40 assertions; was 93/0/0)
- ◀ **Tests: 58%** (M03 closed; was 57%)
- ◀ Committed: `ef14d000e`

### Session 2026-05-26 — B22: finish_reason tracking

- ✅ **B22: finish_reason in stream chunks** — 20 change sites across 10 files
  - Added `finish_reason[32]` to `provider_response_t` + `llm_response_t`
  - All 8 OpenAI-compat providers extract from `choices[0].finish_reason`
  - Anthropic extracts from `message_delta.delta.stop_reason`
  - Google extracts from `candidate.finishReason`
  - Forwarded through stream handlers (`on_provider_stream_chunk`, `on_stream_chunk`)
- ✅ **Bugfix: Google parse_stream_chunk ordering** — checked finishReason after early
  text return, so final chunks with both text and finishReason lost the finish reason.
  Reordered to check finishReason first.
- ✅ **test_finish_reason.c** — 12 assertions across 6 providers
- ◀ **Suite: 91/0/0** (+1 test, 12 assertions)
- ◀ **Providers: 21 provider-specific API gaps remain**
- ◀ Committed: `12a635e64`

### Session 2026-05-26 — B30-B31: Google provider depth

- ✅ **B30: top_k + candidate_count** — `agent.top_k: 40`, `HERMES_TOP_K`, YAML/env/config/diff/agent/llm_client wire. Google writes `topK`/`candidateCount` to generationConfig when > 0.
  - Google provider already had B31 (systemInstruction, verified with test)
- ✅ **test_google_depth.c** — 7 assertions
- ◀ **Suite: 90/0/0** (+1 test, 7 assertions)
- ◀ **Providers: 22 provider-specific API gaps remain**
- ◀ Committed: `5662b2004`

### Session 2026-05-26 — B23: json_mode + response_format UAF fix

- ✅ **B23: json_mode convenience flag** — `agent.json_mode: true` auto-sets response_format to `{"type":"json_object"}` across all 9 providers
  - YAML key: `agent.json_mode` (bool, default false)
  - Env var: `HERMES_JSON_MODE` (0/1/false/true)
  - Wire: config defaults, YAML parse, env override, diff, agent state init, llm_client forwarding
- ✅ **Bugfix: pre-existing use-after-free in response_format path** — All 9 providers had `json_object_set(root, "response_format", rf); json_free(rf)` freeing a ref still in the tree. Fixed with `json_copy(rf)`. Same fix for Anthropic/Google (`json_set`). Bug existed since feature was added.
- ✅ **test_json_mode.c** — 10 assertions, ASan-clean
- ◀ **Suite: 89/0/0** (+1 test, 10 assertions)
- ◀ **Providers: +1 gap (B23), 24 provider-specific API gaps remain**
- ◀ Committed: `4e116f85b`

### Session 2026-05-24 — Config depth: A04 !include directive

- ✅ **A04: `!include path.yaml` directive** — `config_resolve_includes()` preprocesses YAML files before parsing, inlining referenced files with correct indentation. Relative path resolution. Fallback to yaml_parse_file() if no includes found.
- ◀ **Config: 97% → 98%** (A04 closed, 2 depth gaps remain: A05 watching, A06 migration)
- ◀ Committed: `610e94a05`

### Session 2026-05-24 — Docs overhaul: 400-gap-roadmap synced to reality

- ✅ **400-gap-mega-roadmap.md updated** — A, C, E, K, N, O categories corrected to real percentages. Total gap count: ~380 at ~55% parity.
- ◀ Suite: 88/0/0 (no regression)
- ◀ Committed: `TBD`

### Session 2026-05-24 — Config depth: A03 env var expansion

- ✅ **A03: `${VAR:-default}` env var expansion** — `config_expand_env_vars()` scans strings for `${VAR}` and `${VAR:-default}` patterns, substitutes env var values or defaults. Applied to 4 critical YAML keys: model.default, model.provider, model.base_url, model.api_key.
- ✅ **Prestige docs updated** — goal-mantra.md + prestige_prompt.md v7 now reflect real current state (not stale May 22). P0 gaps all marked done.
- ◀ **Suite: 88/0/0** (no regression)
- ◀ **Config: 96% → 97%** (A03 closed, 4 depth gaps remain)
- ◀ Committed: `856ab9722`

### Session 2026-05-24 — Protobuf library test (test_protobuf.c)

- ✅ **test_protobuf.c** — 63 assertions: decode varint (single/multi/truncated/null), varint_size (0/UINT64_MAX), encode varint round-trip (10 values), tag encode/parse, skip field (all 4 wire types + invalid), varint field encode+find, delimited field encode+find (with/without data), fixed32 field encode+find, multi-field scan (3 fields), wrong tag/wire type, empty buffer, null safety
- ◀ **Suite: 88/0/0** (+1 test, 63 assertions; was 87/0/0)
- ◀ **Tests: 56%** (2/42 gaps closed)
- ◀ Committed: `c2c471e78`

### Session 2026-05-24 — Cron library test (test_cron_lib.c)

- ✅ **test_cron_lib.c** — 51 assertions: parse specials, wildcards, specific, step (*/5, */2), ranges, month/day names, comma-separated, error cases (NULL/empty/too few/invalid name/negative step), match (wildcard/specific/weekday/step/date), null safety, next (hourly/daily/*/5/*/2), exact-skip, describe (Every minute/Cron:/At/invalid), cron_free null safety
- ✅ **Know bug found:** cron_next stale tm_wday — dom/dow check before mktime means dow-specific schedules fail
- ◀ **Suite: 87/0/0** (+1 test, 51 assertions; was 86/0/1)
- ◀ Committed: `TBD`

### Session 2026-05-24 — Redact heap overflow fix + test

- ✅ **Bugfix: heap overflow in hermes_redact** — `strndup` allocated exact-size buffer, but `redact_value` expansion (`***REDACTED***` = 15 chars vs shorter values) wrote past buffer. Fixed: `malloc(len + 512)`.
- ✅ **Bugfix: double-counted pointer advancement** — `adj` offset added AFTER `sizeof("***REDACTED***") - 1` already positioned correctly. Removed from both builtin and custom pattern loops. Could cause skipped text or read-past-buffer.
- ✅ **test_redact.c** — 17 assertions: NULL/empty/plain, api_key, sk-, ghp_, token, password, Bearer+JWT, multiple secrets, large input, custom patterns, lifecycle, error handling
- ◀ **Suite: 86/0/1** (+1 test, 17 assertions)
- ◀ **Tests: 54%** (was 54%, 1/43 gaps closed + bugfix)
- ◀ Committed: `2801a39c6`

### Session 2026-05-24 — Build/doc infrastructure (cont)

- ✅ **.dockerignore** — Exclude Python/node/website artifacts from Docker context.
- ✅ **Man page** — `hermes.1`: full man page with 14 CLI options, 12 slash commands, config/plugin/gateway docs, env vars, exit codes.
- ✅ **Build/doc: 55%** (was 30%, 5/15 gaps closed: Dockerfile + CI + cross-compile + .dockerignore + man page)
- ◀ 5 build gaps closed this session
- ◀ Committed: `20edb4d35`

### Session 2026-05-24 — Build/doc infrastructure

- ✅ **Dockerfile** — Multi-stage build: gcc:13-bookworm builder → debian:bookworm-slim runtime (~20MB). Stripped binary, libssl3 + curl for plugin HTTP calls.
- ✅ **CI workflow** — GitHub Actions: build + smoke test + test suite + TUI + plugins + Docker build + image size check. Path-filtered to C/ changes.
- ✅ **Cross-compile script** — `scripts/cross-compile.sh`: linux-x86_64, linux-aarch64, linux-armv7, windows-x86_64. Auto-detects toolchain, fails gracefully with install instructions.
- ◀ **Build/doc: 50%** (was 30%, 3/15 gaps closed)
- ◀ Committed: `a61cac0fd`

### Session 2026-05-24 (Spotify plugin real + test)

- ✅ **Spotify plugin real** — `plugin_spotify.c` upgraded from stub to real Spotify Web API integration:
  - Client Credentials OAuth flow via `popen("curl ...")` — no external library deps
  - Real endpoints: play (PUT /me/player/play), pause (PUT /me/player/pause), next (POST /me/player/next), current-playing (GET /me/player/currently-playing), search (GET /search)
  - Token management: auto-refresh, expiry tracking, 60s safety buffer
  - Config via plugin_configure(): client_id, client_secret, device_id
  - Graceful error: "authentication failed" when unconfigured, parse errors on bad API response
  - Plugin version 1.0.0
- ✅ **test_plugin_spotify.c** — 18 assertions covering metadata, interface ptrs, error handling with/without config
- ✅ **Suite: 84/0/1** (+1 pass)
- ✅ **Plugins: 14%** (all 3 .so now real — 0 remaining stubs)
- ◀ Committed: `2198e92dd`

### Session 2026-05-24 (Kanban plugin real + test)

- ✅ **Kanban plugin real** — `plugin_kanban.c` upgraded from stub to full in-memory board with task CRUD:
  - 8 boards × 256 tasks per board in static storage
  - Real task storage: description, column (todo/in_progress/done/blocked), priority (1-5), assignee, sticky flag
  - JSON output with proper escaping via `json_escape()`
  - Error handling: max boards, board full, null name, invalid board ID
  - All 4 interface functions: create_board, add_task, get_board, list_boards
  - Plugin version 1.0.0
- ✅ **test_plugin_kanban.c** — 45 assertions covering create, list, add task (×3), get board, error paths, cleanup
- ✅ **test_runner.sh** — kanban plugin test added
- ◀ **Suite: 83/0/1** ✅ (+1 pass)
- ◀ **Plugins: ~12%** (1 more real plugin, was 10%)
- ◀ Committed: `ddede70b9`

## Session 2026-05-22 (Current)

- ✅ **G55: MCP SSE transport** — Replaced "not yet implemented" stub with real `connect_sse_server()`. Uses `mcp_server_set_sse()` + HTTP/SSE transport with auth header injection, root directories, tool filtering, and dynamic handler registration. Same pattern as stdio path.
- ◀ **Suite: 82/0/1** ✅
- ◀ Committed: `2216e98b4`

- ✅ **Phase 113: Agent loop retry + fallback** — LLM call retry loop with exponential backoff (1s-16s). Wire `api_max_retries` (default 3), `fallback_model`, `fallback_providers` from config → llm_config_t. Fallback model first, then comma-separated fallback providers. Test: 9 new assertions.
- ◀ **Suite: 82/0/1** (config test now 79 assertions)
- ◀ Committed: `b667db689`

- ✅ **M31: File tool test** — test_file.c, 35 assertions for file CRUD (read/write/search) + sandbox enforcement. Covers: create, overwrite, parent dirs, empty file, missing path, offset/limit read, non-existent file, null args, pattern/glob search, path traversal /etc blocking, 500B content, special path chars
- ✅ **-Werror fixes in file.c** — `/*` within comment, unused `fread` return
- ✅ **test_runner.sh updated** — file_tool (35 tests) added
- ◀ **Tests now 28 files, 1,473 assertions. Suite: 59/59 pass, 0 fail, 3 skip**
- ◀ Committed: `2ebd96df5`

### Session 2026-05-22 (Later — L01: BSM Secrets Manager)
- ✅ **L01: Bitwarden Secrets Manager** — New `secrets` subsystem with config struct (`secrets_config_t`), `include/hermes_secrets.h` header, `src/secrets.c` implementation
  - Config: YAML keys `secrets.bitwarden.{enabled,access_token,organization_id,bws_path,install_timeout}`, env vars `HERMES_SECRETS_BITWARDEN_*`
  - Full config wiring: defaults, YAML parsing, env override, diff, export, merge, validation
  - Runtime: lazy bws install (curl download + chmod), BSM auth via `bws project list`, secret resolution `${BSM:name}` in config values
- ◀ Committed: (current commit)

### Session 2026-05-22 (Evening — L07: xAI Encrypted Reasoning Replay)
- ✅ **L07: xAI encrypted reasoning replay** — Thread encrypted_content across conversation turns
  - Added `encrypted_content` field to message_t, provider_response_t, llm_response_t
  - Parse from xAI response messages (non-stream + stream path)
  - Include in xAI request body for assistant messages on subsequent calls
  - Generic llm_client parser also handles encrypted_content field
  - Full message lifecycle: new, clone, free all handle encrypted_content
  - Wire through agent_loop assistant message creation
- ◀ Committed: `17a88748f`

## Session 2026-05-22 (L03 + M30)

- ✅ **L03: xAI Web Search** — search_xai() via Responses API + web_search tool dispatch
- ✅ **M30: Web tool test** — test_web.c, 22 assertions for all 3 web handlers + 6 backend dispatch
- ✅ **Bugfix: use-after-free in web_search_handler** — backend arg read after json_free(args) caused all backends to silently fall through to default searxng
- ◀ **Tests: 36 files, 6,920 lines, 69 passed, 0 failed, 0 skipped**
- ◀ Committed: `46b284ac1`
- ◀ **~315 gaps remaining** (2 closed this session)
