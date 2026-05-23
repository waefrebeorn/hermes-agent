     1|# State — Hermes C Translation (2026-05-27)
     2|
     3|## ~63% toward 1:1 Python parity (~338 gaps remaining)
     4|
     5|### Milestone Dashboard
     6|
     7|| Category | Gaps | Done% | Key Stats |
     8||----------|------|-------|-----------|
     9|| **Config** | 2 depth | 98% | 322/322 YAML keys, profiles, `${VAR}`, `!include` |
| **Providers** | 35 | 87% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 16 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve |
|| **Plugins** | 48 | **22%** | 10 .so: kanban + honcho + spotify + disk-cleanup + file-memory + achievements + observability + skills + image_gen + google_meet |
| **Gateway** | 63 | **100% ✅** | All 63 gaps closed |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 31 | 86% | 23 state fields, 18 session DB, G01-G36 all filled |
| **CLI** | 33 | 87% | Spinner implemented. Skin engine active. 74 commands |
||| **Libs** | 8 | **70%** | libpath + libdatetime + libcsv + libhash + libuuid + libbase64 + libhtml + libtextwrap + libglob + libsignal + libenum + libdifflib + libregex (J04-J16) |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
|| **Tests** | 34 | **66%** | **103 files, ~3,000+ assertions** (140/0/0 suite) |
| **Upstream** | 1 | new | L02 remains (CDP auto-launch, blocked) (125 commits behind) |
|| **Cross-cut** | 4 | **100% (6/6) ✅** | Token counting, secure parent dir, key leakage, vendor key derivation, local trust |
| **Build/doc** | 1 | **95%** | O14 sandbox escape detection done. O02 Windows build remains. |
| **Error types** | 0 | **100% ✅** | K01-K20: 58 error codes complete |
|### Session 2026-05-29 — J18 libwebsocket: test + .a target + NULL ptr bugfix
|- ✅ **J18: libwebsocket** — 14 tests covering ws_connect error paths (NULL/empty/http/invalid), ws_close NULL safety, ws_frame_free lifecycle, ws_send/ws_recv NULL safety. No network deps — tests exercise error-handling paths only.
|- ✅ **Bugfix: ws_connect(NULL) SEGV** — parse_url called strncmp(NULL, "wss://", 6), crashing. Added !url check before parse_url.
|- ✅ **Makefile** — libwebsocket.a added to LIB_A, build rules for .o + .a
|- ✅ **test_runner.sh** — websocket test registered
|- ◀ **Suite: 140/0/0 → 141/0/0** (+1)
|- ◀ **Libs: 70% → 72%** (26 archives, J04-J18 done)
|- ◀ 1 commit pushed: `test(lib): add J18 libwebsocket test + .a target`
|### Session 2026-05-28 — J11 libtextwrap + fallback_routing test + google_meet wiring
- ✅ **J11: libtextwrap** — textwrap_wrap, textwrap_fill, textwrap_dedent, textwrap_shorten (35 assertions, 133/0/0)
- ✅ **P83: test_fallback_routing.c** — 93 assertions covering cool-off, advance, tick, stats, auth/rate-limit special handling
- ✅ **test_plugin_google_meet.c wired** — Google Meet plugin test (6 assertions) registered in runner
- ◀ **Suite: 126/0/0 → 133/0/0** (+7)
- ◀ **Libs: 52% → 55%** (21 .a, J11 done: textwrap)
- ◀ **Tests: 66% → 66%** (95 files, ~2,700 assertions)
- ◀ 3 commits pushed: google_meet wiring, fallback_routing test, J11 libtextwrap
- ◀ **Error types: 100% ✅** (K01-K20, 58 codes)
- ◀ **Plugins: 22%** (10 .so)

### Session 2026-05-28 — google_meet test wiring + state.md update
- ✅ **test_plugin_google_meet.c wired** — Google Meet plugin test (6 assertions) registered in runner
- ◀ **Suite: 130/0/0 → 131/0/0** (+1)
- ◀ 1 commit pushed: test(plugin): wire google_meet test in runner

### Session 2026-05-28 — Achievements plugin + test wiring (D10)
- ✅ **D10: plugin_achievements** — Achievement tracking plugin: tiered achievements for tool usage, sessions, debugging feats (PLUGIN_ACHIEVEMENTS)
- ✅ **tests/test_plugin_achievements.c** — 10 assertions covering defs, metrics, tier eval, secrets, persistence, tools
- ◀ **Suite: 126/0/0 → 127/0/0** (+1)
- ◀ **Plugins: 19% → 22%** (6 .so, 1 new: achievements)
- ◀ 1 commit pushed to waefrebeorn/hermes-agent main
### Session 2026-05-28 — libhash + libuuid + libbase64 + plugins + test wiring (J07-J09, D08-D09)
- ✅ **J07: libhash** — SHA-256/SHA-1/MD5/HMAC hashing (25 assertions, 118/0/0)
- ✅ **J08: libuuid** — UUID v4/v5 generation, parsing, validation (60 assertions, 119/0/0)
- ✅ **Tests: 3 orphan tests wired** — test_finish_reason, test_google_depth, test_json_mode (now active, +3, 122/0/0)
- ✅ **D08: plugin_disk_cleanup** — Disk cleanup utility plugin: disk_usage, disk_clean_temp, disk_status (PLUGIN_DISK_CLEANUP)
- ✅ **D09: plugin_file_memory** — Persistent file-backed memory provider: store/search/clear with JSON-lines file (PLUGIN_MEMORY)
- ✅ **J09: libbase64** — RFC 4648 base64/url encode/decode with padding, validation (34 assertions, 125/0/0)
- ◀ **Suite: 117/0/0 → 125/0/0** (+8 over previous 117)
- ◀ **Plugins: 14% → 19%** (5 .so, 2 new: disk-cleanup + file-memory)
- ◀ **Libs: 41% → 52%** (19 archives, 3 new: hash + uuid + base64)
- ◀ **Tests: 64% → 66%** (86 test files, ~2,400 assertions)
- ◀ 5 commits pushed to waefrebeorn/hermes-agent main
### Session 2026-05-27 — libdatetime: C datetime library (J05)
- ✅ **lib/libdatetime/datetime.{h,c}** — New standalone library porting Python's datetime:
  - `datetime_now()`, `datetime_from_time_t()`, `datetime_from_time_t_utc()` — constructors
  - `datetime_parse_iso8601()` — full ISO 8601 parsing (T/Z/space/offset/date-only)
  - `datetime_format()` / `datetime_format_utc()` — strftime wrappers
  - `datetime_describe()` — relative time description ("just now", "2 hours ago", "yesterday", "3 days ago", future)
  - `datetime_age_seconds()` / `_minutes()` / `_hours()` / `_days()` — age queries
  - `datetime_add_days()` / `_hours()` / `_seconds()` — date math
  - `datetime_is_expired()` — TTL expiry check
  - `datetime_is_today()` / `_is_yesterday()` / `_same_day()` / `_day_start()` — calendar queries
  - `datetime_buffer()` — no-alloc ISO 8601 formatting
- ✅ **tests/test_datetime.c** — 59 assertions covering all 20 public functions + edge cases
- ✅ **Makefile** — libdatetime.a added to libs target (15 archives)
- ✅ **test_runner.sh** — datetime test registered
- ✅ Suite: **116/0/0** (+1 test file, 59 assertions; was 115/0/0)
- ✅ Libs: **35%→38%** (1/12 remaining gaps closed: J05)
### Session 2026-05-27 — libpath: C path manipulation library (J04)
- ✅ **lib/libpath/path.{h,c}** — New standalone library porting Python pathlib:
  - `path_join()`, `path_join_n()` — join path components with '/'
  - `path_basename()`, `path_dirname()` — filename and parent extraction
  - `path_stem()`, `path_suffix()`, `path_suffixes()` — extension handling
  - `path_ext_swap()` — replace extension (with/without leading dot)
  - `path_normalize()` — collapse ".", "..", double slashes
  - `path_resolve()`, `path_abs()`, `path_is_absolute()` — path resolution
  - `path_exists()`, `path_is_dir()`, `path_is_file()` — stat queries
  - `path_glob()`, `path_fnmatch()` — pattern matching
- ✅ **tests/test_path.c** — 76 assertions covering all 17 public functions + edge cases
- ✅ **Makefile** — libpath.a static archive added to build + test-libs target
- ✅ **test_runner.sh** — path test registered
- ✅ Suite: **114/0/0** (+1 test file, 76 assertions; was 113/0/0)
- ✅ Libs: **20%→35%** (1/14 gaps closed: J04)
### Session 2026-05-26 — File permission hardening (O15)
- ✅ **hermes_file_permissions_harden()** — Function that sets 0600/0700 on home dir, config.yaml, .env, session DB, vault.dat, cron store
- ✅ **cron_sqlite.c: 0644→0600** — Cron store now secures to 0600 (contained API keys in job configs)
- ✅ **Wired in CLI init** — Called after config load in hermes_cli_main
- ✅ **test_file_permissions.c** — 15 assertions: basic hardening (6 files), root-skip (2), NULL/edge safety (3), vault paths (2)
- ✅ Skips when uid==0 (root), NULL-safe, empty-string safe, missing-file safe
- ◀ **Build/doc: 75%→77%** (1/7 gaps closed: O15)
- ◀ **Suite: 99/0/0** (+1 test, 15 assertions; was 98/0/0)
### Session 2026-05-26 — Vault encryption at rest test (O11)
- ✅ **test_vault.c** — 37 assertions covering full vault lifecycle:
  - Master key set/lock/has, store/retrieve/delete credentials
  - Persistence: save encrypted file, reload, verify data survives
  - Update existing credential, persisted update after reload
  - List services (deduped), wrong-key decryption fails gracefully
  - NULL safety (9 edge cases)
- ✅ **Bugfix: vault_set_master_key didn't set g_vault_unlocked** — retrieve returned NULL after set_master_key+store because unlocked flag was only set on vault_load()
- ◀ **Build/doc: 77%→80%** (1 gap closed: O11)
- ◀ **Suite: 100/0/0** (+1 test, 37 assertions; was 99/0/0)
### Session 2026-05-26 — Release automation (O05)
- ✅ **scripts/release.sh** — Automated release script: version bump (patch/minor/major), build, run tests, update CHANGELOG, git commit + tag
   100|- ✅ **Makefile `release` target** — `make release [patch|minor|major]`
   101|- ✅ Does NOT auto-push — caller must `git push --tags origin main`
   102|- ◀ **Build/doc: 80%→83%** (1 gap closed: O05)
   103|- ◀ Committed: pending
   104|
   105|### Session 2026-05-26 — Audit log rotation (O12)
   106|
   107|- ✅ **audit_check_rotate()** — Auto-rotate when log exceeds max_size (configurable: max_size_kb, max_files, max_age_days)
   108|- ✅ **audit_set_rotation()** — Public API for setting rotation params
   109|- ✅ **Shift + expiry** — Rotated files shift (1→2, 2→3…), expired files deleted by mtime
   110|- ✅ **Wired in CLI init** — 10MB max, 5 rotated files, 30-day expiry defaults
   111|- ✅ **test_audit_rotate.c** — 11 assertions: init, log write, content verification, config changes
   112|- ✅ Fix: added missing `#include <unistd.h>` for `unlink()`
   113|- ◀ **Build/doc: 83%→86%** (1 gap closed: O12)
   114|- ◀ **Suite: 101/0/0** (+1 test, 11 assertions; was 100/0/0)
   115|
   116|### Session 2026-05-26 — exec_code tool test (M41)
   117|
   118|- ✅ **test_exec_code.c** — 16 assertions: stdout capture, NULL/empty code, multi-line, stderr redirect, exit code 0
   119|- ✅ Self-contained test (no library deps), exercises Python subprocess invocation like real exec_code_handler
   120|- ◀ **Tests: 60%→61%** (1 gap closed: M41)
   121|- ◀ **Suite: 102/0/0** (+1 test, 16 assertions; was 101/0/0)
   122|
   123|### Session 2026-05-26 — Release automation (O05)
   124|
   125|- ✅ **scripts/release.sh** — Automated release script: version bump (patch/minor/major), build, run tests, update CHANGELOG, git commit + tag
   126|- ✅ **Makefile `release` target** — `make release [patch|minor|major]`
   127|- ✅ Does NOT auto-push — caller must `git push --tags origin main`
   128|- ◀ **Build/doc: 80%→83%** (1 gap closed: O05)
   129|- ◀ Committed: pending
   130|
   131|### Session 2026-05-26 — Architecture documentation (O08)
   132|
   133|- ✅ **ARCHITECTURE.md** — Full system overview with ASCII diagram: CLI/Gateway → Agent Loop → Providers → Config/Plugins → Library stack
   134|- ✅ Covers: data flow, startup sequence, module architecture (config 322+ fields, 9 providers, 28 tools, 19 gateway platforms, 14 libraries), design decisions, testing philosophy
   135|- ◀ **Build/doc: 75%** (was 70%, 1/7 gaps closed: O08)
   136|- ◀ Committed: `3f53e0154`
   137|
   138|### Session 2026-05-26 — Config env priority tests (M23)
   139|
   140|- ✅ **test_config.c: 99 assertions** (+10 M23 env priority tests)
   141|- ✅ Tests: numeric/float/string/boolean env overrides, empty/non-existent
   142|- ◀ **Tests: 64%** (1/34 gaps closed: M23)
   143|- ◀ **Config: 98%** (env override priority chain verified)
   144|- ◀ Committed: `66923f2ed`
   145|
   146|### Session 2026-05-26 — Config validation edge cases (M22)
   147|
   148|- ✅ **test_config.c: 89 assertions** (+10 new M22 edge cases)
   149|- ✅ **Bugfix: hermes_config_validate** — NULL cfg/result no longer SEGV
   150|- ✅ **Bugfix: add_issue(NULL)** — guarded against null result pointer
   151|- ◀ **Tests: 63%** (1/35 gaps closed: M22)
   152|- ◀ **Config: 98%** (validation function hardened)
   153|- ◀ Committed: `51eb4eedf`
   154|
   155|### Session 2026-05-26 — Gateway escape mode tests (M07)
   156|
   157|- ✅ **test_gateway_escape.c** — 30 assertions covering:
   158|  - `gw_markdown_to_html`: bold/italic/code/HTML entity escaping/mixed/nested
   159|  - `gw_markdown_v2_escape`: all 18 reserved chars, underscore, asterisk, bracket, backtick
   160|  - `gw_truncate_message`: NULL/empty/short/truncation with word boundary/ellipsis/unicode/preserves markdown
   161|- ◀ **Tests: 62%** (1/36 gaps closed: M07)
   162|- ◀ Committed: `TBD`
   163|
   164|### Session 2026-05-26 — SECURITY.md (O09)
   165|
   166|- ✅ **SECURITY.md** — Full disclosure policy, supported versions, 72h acknowledgment commitment
   167|- ✅ Documents: API key protection (`hermes_redact`, `url_safety.c`, `secure_parent_dir`), command sandboxing (bwrap, file sandbox, env isolation), config hardening (0700 on HOME, .env strict parsing), credential rotation (pool, weighted selection)
   168|- ✅ Known security gaps tracked: O11-O15 (encryption at rest, audit rotation, sandbox escape detection, permission hardening)
   169|- ✅ Dependency audit (libjson/libhttp/libyaml/libcrypto/OpenSSL/libcurl/sqlite3) + supply chain practices
   170|- ◀ **Build/doc: 70%** (was 65%, 1/8 gaps closed: O09)
   171|- ◀ Committed: `b58b7e88a`
   172|
   173|### Session 2026-05-26 — M06: Provider error handling edge cases
   174|
   175|- ✅ **test_provider_error.c** — 225 assertions across 9 providers covering:
   176|  - NULL body/empty string/malformed JSON/different JSON types → no crash, non-NULL response
   177|  - Auth error/rate limit/server error → parsed content with descriptive message
   178|  - NULL/empty/malformed stream chunks → no SIGSEGV
   179|  - free_response(NULL) → no crash
   180|  - Provider-specific edge cases: empty choices, empty content blocks, [DONE] markers, finish reason in stream
   181|- ✅ **Bugfix: NULL SIGSEGV in 6 parse_stream_chunk** — openai, openrouter, deepseek, xai, anthropic, custom all lacked `if (!chunk) return;` before `strncmp(chunk, ...)`. Azure/google/bedrock were already safe. All 9 providers now null-safe on stream chunk input.
   182|- ✅ **Bugfix: API error JSON silently dropped in 6 parse_response** — openai, openrouter, deepseek, xai, custom had no `{"error":{...}}` check. Bedrock lacked both raw message and wrapped error handling. All 9 providers now return descriptive error content when API returns error JSON.
   183|- ◀ **Suite: ALL PASSED** (M06 + existing tests verified)
   184|- ◀ **Tests: 61%** (1/37 gaps closed: M06)
   185|- ◀ **Providers: 88%** (error handling bugs fixed across all providers)
   186|- ◀ Committed: `TBD`
   187|
   188|### Session 2026-05-26 — Doxygen API docs infrastructure (O07)
   189|
   190|- ✅ **Doxyfile** — Full project config for Hermes Agent C API documentation
   191|- ✅ **Module doc comments** — Doxygen `\defgroup` tags for 13 key headers (hermes, provider, http, json, agent, db, crypto, yaml, gateway, display, memory, error, tokenizer, skin, plugin)
   192|- ✅ **`make docs` target** — Auto-detects doxygen, generates HTML docs
   193|- ◀ **Build/doc: 65%** (was 60%, O07 closed)
   194|- ◀ Committed: `c906bc5f2`
   195|
   196|### Session 2026-05-26 — CHANGELOG.md (O10)
   197|
   198|- ✅ **CHANGELOG.md** — Full changelog from v0.1.0 (initial scaffold) through v0.7.0 (test/build infrastructure) to unreleased (current provider test coverage). 150 lines covering all 4 phases, 9 major releases, organized by feature area with suite stats per version.
   199|- ◀ **Build/doc: 60%** (was 55%, O10 closed)
   200|- ◀ Committed: `ee65389a3`
   201|
   202|### Session 2026-05-26 — Azure provider full tests + 3 bugfixes
   203|
   204|- ✅ **test_azure_full.c** — 45 assertions covering URL (deployment_id, api_version, trailing slash), headers, 12 LLM params, response_format/json_mode/strict, metadata, tool_choice, parallel_tool_calls, max_tool_calls, n, extra_body, messages (system/user/assistant/tool), tools JSON, response parsing (text/tool_calls/error), streaming (text delta/[DONE]/finish_reason/non-data prefix), null safety
   205|- ✅ **Bugfix: 2x UAF (metadata + tool_choice)** — json_object_set then json_free
   206|- ✅ **Bugfix: Error object not parsed** — added Azure API error handling in parse_response
   207|- ✅ **Bugfix: NULL crash in azure_parse_stream_chunk** — strncmp on NULL
   208|- ◀ **Suite: 96/0/0** (+1 test, 45 assertions; M05 closed)
   209|- ◀ **Tests: 60%** (was 59%, 1/37 gaps closed)
   210|- ◀ Committed: `d38e20f4e`
   211|
   212|### Session 2026-05-26 — Bedrock provider full tests + 3 bugfixes
   213|
   214|- ✅ **test_bedrock_full.c** — 35 assertions covering:
   215|  - URL building (default region us-east-1, custom model)
   216|  - Headers without AWS creds
   217|  - Request body: inferenceConfig, system array, messages, B39-B41
   218|  - response_format, json_mode, tool_choice, metadata
   219|  - Response parsing: text, toolUse (arguments serialized), empty, error
   220|  - Streaming chunk passthrough, null safety
   221|- ✅ **Bugfix: UAF in metadata path** — `json_object_set` then `json_free` on same node
   222|- ✅ **Bugfix: UAF in tool_choice parsing** — same pattern
   223|- ✅ **Bugfix: toolUse input object deserialized as string** — `json_get_str` returned `{}` for object. Changed to `json_serialize`.
   224|- ◀ **Suite: 95/0/0** (+1 test, 35 assertions; was 94/0/0)
   225|- ◀ **Tests: 59%** (2 gaps closed: M03-M04)
   226|- ◀ Committed: `b0a6c38b5`, `ef14d000e`, `6f0a44091`
   227|
   228|### Session 2026-05-26 — Anthropic provider depth tests (B26-B28)
   229|
   230|- ✅ **test_anthropic_depth.c** — 80 assertions covering:
   231|  - URL building (default, custom, trailing slash, /messages suffix)
   232|  - Headers (with/without API key, x-api-key format vs Bearer)
   233|  - Request body: model, max_tokens, temperature, top_p, stop sequences
   234|  - **B26: Thinking blocks** — reasoning_effort low/medium/high/direct number, budget_tokens mapping
   235|  - **B27: Cache control** — last user message content block + system prompt on first call, plain string on cached
   236|  - System message extraction to top-level "system" field, concatenation of multiple system messages
   237|  - Tool format conversion: OpenAI's `{\\"type\\":\\"function\\",\\"function\\":{...}}` → Anthropic's `{\\"name\\":\\"...\\",\\"input_schema\\":{...}}`
   238|  - Response parsing: text, thinking/reasoning, tool_use, error objects, usage tracking
   239|  - Streaming: message_start, content_block_delta (text_delta + thinking_delta), content_block_start, message_delta (stop_reason), direct data: format
   240|  - response_format, json_mode, tool_choice, parallel_tool_calls, extra_body merging
   241|  - Null safety: NULL chunk, NULL response body, empty content blocks
   242|- ✅ **Bugfix: NULL crash in anthropic_parse_stream_chunk** — added null safety before strncmp dereference
   243|- ◀ **Suite: 93/0/0** (+1 test, 80 assertions; was 92/0/0)
   244|- ◀ **Tests: 57%** (was 56%, 1/40 test gaps closed)
   245|- ◀ **Providers: 87%** (B26-B28 coverage verified)
   246|- ◀ Committed: `b0a6c38b5`
   247|
   248|### Session 2026-05-26 — Google provider full tests + 3 bugfixes
   249|
   250|- ✅ **test_google_full.c** — 40 assertions covering:
   251|  - URL building (default, custom, trailing slash fix, :generateContent)
   252|  - Headers with/without API key
   253|  - Generation config: maxOutputTokens, temperature, topP, stopSequences, topK, candidateCount
   254|  - B29: safety_settings JSON array parsing
   255|  - System instruction extraction to systemInstruction with parts array
   256|  - OpenAI→Google tool format conversion: functionDeclarations wrapper fix
   257|  - response_format, json_mode, tool_choice, parallel_tool_calls, extra_body
   258|  - Response parsing: text, functionCall, empty candidates, error
   259|  - Streaming: text delta, finishReason, [DONE] marker, non-data prefix
   260|  - Null safety: free_response, NULL/empty/invalid
   261|- ✅ **Bugfix: Google tools functionDeclarations set on array** — `json_set(tools_arr, "functionDeclarations", ...)` on array was a no-op. Added wrapper object + append.
   262|- ✅ **Bugfix: Trailing slash → //models** — stripped trailing slash in URL builder.
   263|- ✅ **Bugfix: NULL crash in google_parse_stream_chunk** — strncmp on NULL.
   264|- ◀ **Suite: 94/0/0** (+1 test, 40 assertions; was 93/0/0)
   265|- ◀ **Tests: 58%** (M03 closed; was 57%)
   266|- ◀ Committed: `ef14d000e`
   267|
   268|### Session 2026-05-26 — B22: finish_reason tracking
   269|
   270|- ✅ **B22: finish_reason in stream chunks** — 20 change sites across 10 files
   271|  - Added `finish_reason[32]` to `provider_response_t` + `llm_response_t`
   272|  - All 8 OpenAI-compat providers extract from `choices[0].finish_reason`
   273|  - Anthropic extracts from `message_delta.delta.stop_reason`
   274|  - Google extracts from `candidate.finishReason`
   275|  - Forwarded through stream handlers (`on_provider_stream_chunk`, `on_stream_chunk`)
   276|- ✅ **Bugfix: Google parse_stream_chunk ordering** — checked finishReason after early
   277|  text return, so final chunks with both text and finishReason lost the finish reason.
   278|  Reordered to check finishReason first.
   279|- ✅ **test_finish_reason.c** — 12 assertions across 6 providers
   280|- ◀ **Suite: 91/0/0** (+1 test, 12 assertions)
   281|- ◀ **Providers: 21 provider-specific API gaps remain**
   282|- ◀ Committed: `12a635e64`
   283|
   284|### Session 2026-05-26 — B30-B31: Google provider depth
   285|
   286|- ✅ **B30: top_k + candidate_count** — `agent.top_k: 40`, `HERMES_TOP_K`, YAML/env/config/diff/agent/llm_client wire. Google writes `topK`/`candidateCount` to generationConfig when > 0.
   287|  - Google provider already had B31 (systemInstruction, verified with test)
   288|- ✅ **test_google_depth.c** — 7 assertions
   289|- ◀ **Suite: 90/0/0** (+1 test, 7 assertions)
   290|- ◀ **Providers: 22 provider-specific API gaps remain**
   291|- ◀ Committed: `5662b2004`
   292|
   293|### Session 2026-05-26 — B23: json_mode + response_format UAF fix
   294|
   295|- ✅ **B23: json_mode convenience flag** — `agent.json_mode: true` auto-sets response_format to `{"type":"json_object"}` across all 9 providers
   296|  - YAML key: `agent.json_mode` (bool, default false)
   297|  - Env var: `HERMES_JSON_MODE` (0/1/false/true)
   298|  - Wire: config defaults, YAML parse, env override, diff, agent state init, llm_client forwarding
   299|- ✅ **Bugfix: pre-existing use-after-free in response_format path** — All 9 providers had `json_object_set(root, "response_format", rf); json_free(rf)` freeing a ref still in the tree. Fixed with `json_copy(rf)`. Same fix for Anthropic/Google (`json_set`). Bug existed since feature was added.
   300|- ✅ **test_json_mode.c** — 10 assertions, ASan-clean
   301|- ◀ **Suite: 89/0/0** (+1 test, 10 assertions)
   302|- ◀ **Providers: +1 gap (B23), 24 provider-specific API gaps remain**
   303|- ◀ Committed: `4e116f85b`
   304|
   305|### Session 2026-05-24 — Config depth: A04 !include directive
   306|
   307|- ✅ **A04: `!include path.yaml` directive** — `config_resolve_includes()` preprocesses YAML files before parsing, inlining referenced files with correct indentation. Relative path resolution. Fallback to yaml_parse_file() if no includes found.
   308|- ◀ **Config: 97% → 98%** (A04 closed, 2 depth gaps remain: A05 watching, A06 migration)
   309|- ◀ Committed: `610e94a05`
   310|
   311|### Session 2026-05-24 — Docs overhaul: 400-gap-roadmap synced to reality
   312|
   313|- ✅ **400-gap-mega-roadmap.md updated** — A, C, E, K, N, O categories corrected to real percentages. Total gap count: ~380 at ~55% parity.
   314|- ◀ Suite: 88/0/0 (no regression)
   315|- ◀ Committed: `TBD`
   316|
   317|### Session 2026-05-24 — Config depth: A03 env var expansion
   318|
   319|- ✅ **A03: `${VAR:-default}` env var expansion** — `config_expand_env_vars()` scans strings for `${VAR}` and `${VAR:-default}` patterns, substitutes env var values or defaults. Applied to 4 critical YAML keys: model.default, model.provider, model.base_url, model.api_key.
   320|- ✅ **Prestige docs updated** — goal-mantra.md + prestige_prompt.md v7 now reflect real current state (not stale May 22). P0 gaps all marked done.
   321|- ◀ **Suite: 88/0/0** (no regression)
   322|- ◀ **Config: 96% → 97%** (A03 closed, 4 depth gaps remain)
   323|- ◀ Committed: `856ab9722`
   324|
   325|### Session 2026-05-24 — Protobuf library test (test_protobuf.c)
   326|
   327|- ✅ **test_protobuf.c** — 63 assertions: decode varint (single/multi/truncated/null), varint_size (0/UINT64_MAX), encode varint round-trip (10 values), tag encode/parse, skip field (all 4 wire types + invalid), varint field encode+find, delimited field encode+find (with/without data), fixed32 field encode+find, multi-field scan (3 fields), wrong tag/wire type, empty buffer, null safety
   328|- ◀ **Suite: 88/0/0** (+1 test, 63 assertions; was 87/0/0)
   329|- ◀ **Tests: 56%** (2/42 gaps closed)
   330|- ◀ Committed: `c2c471e78`
   331|
   332|### Session 2026-05-24 — Cron library test (test_cron_lib.c)
   333|
   334|- ✅ **test_cron_lib.c** — 51 assertions: parse specials, wildcards, specific, step (*/5, */2), ranges, month/day names, comma-separated, error cases (NULL/empty/too few/invalid name/negative step), match (wildcard/specific/weekday/step/date), null safety, next (hourly/daily/*/5/*/2), exact-skip, describe (Every minute/Cron:/At/invalid), cron_free null safety
   335|- ✅ **Know bug found:** cron_next stale tm_wday — dom/dow check before mktime means dow-specific schedules fail
   336|- ◀ **Suite: 87/0/0** (+1 test, 51 assertions; was 86/0/1)
   337|- ◀ Committed: `TBD`
   338|
   339|### Session 2026-05-24 — Redact heap overflow fix + test
   340|
   341|- ✅ **Bugfix: heap overflow in hermes_redact** — `strndup` allocated exact-size buffer, but `redact_value` expansion (`***REDACTED***` = 15 chars vs shorter values) wrote past buffer. Fixed: `malloc(len + 512)`.
   342|- ✅ **Bugfix: double-counted pointer advancement** — `adj` offset added AFTER `sizeof("***REDACTED***") - 1` already positioned correctly. Removed from both builtin and custom pattern loops. Could cause skipped text or read-past-buffer.
   343|- ✅ **test_redact.c** — 17 assertions: NULL/empty/plain, api_key, sk-, ghp_, token, password, Bearer+JWT, multiple secrets, large input, custom patterns, lifecycle, error handling
   344|- ◀ **Suite: 86/0/1** (+1 test, 17 assertions)
   345|- ◀ **Tests: 54%** (was 54%, 1/43 gaps closed + bugfix)
   346|- ◀ Committed: `2801a39c6`
   347|
   348|### Session 2026-05-24 — Build/doc infrastructure (cont)
   349|
   350|- ✅ **.dockerignore** — Exclude Python/node/website artifacts from Docker context.
   351|- ✅ **Man page** — `hermes.1`: full man page with 14 CLI options, 12 slash commands, config/plugin/gateway docs, env vars, exit codes.
   352|- ✅ **Build/doc: 55%** (was 30%, 5/15 gaps closed: Dockerfile + CI + cross-compile + .dockerignore + man page)
   353|- ◀ 5 build gaps closed this session
   354|- ◀ Committed: `20edb4d35`
   355|
   356|### Session 2026-05-24 — Build/doc infrastructure
   357|
   358|- ✅ **Dockerfile** — Multi-stage build: gcc:13-bookworm builder → debian:bookworm-slim runtime (~20MB). Stripped binary, libssl3 + curl for plugin HTTP calls.
   359|- ✅ **CI workflow** — GitHub Actions: build + smoke test + test suite + TUI + plugins + Docker build + image size check. Path-filtered to C/ changes.
   360|- ✅ **Cross-compile script** — `scripts/cross-compile.sh`: linux-x86_64, linux-aarch64, linux-armv7, windows-x86_64. Auto-detects toolchain, fails gracefully with install instructions.
   361|- ◀ **Build/doc: 50%** (was 30%, 3/15 gaps closed)
   362|- ◀ Committed: `a61cac0fd`
   363|
   364|### Session 2026-05-24 (Spotify plugin real + test)
   365|
   366|- ✅ **Spotify plugin real** — `plugin_spotify.c` upgraded from stub to real Spotify Web API integration:
   367|  - Client Credentials OAuth flow via `popen("curl ...")` — no external library deps
   368|  - Real endpoints: play (PUT /me/player/play), pause (PUT /me/player/pause), next (POST /me/player/next), current-playing (GET /me/player/currently-playing), search (GET /search)
   369|  - Token management: auto-refresh, expiry tracking, 60s safety buffer
   370|  - Config via plugin_configure(): client_id, client_secret, device_id
   371|  - Graceful error: "authentication failed" when unconfigured, parse errors on bad API response
   372|  - Plugin version 1.0.0
   373|- ✅ **test_plugin_spotify.c** — 18 assertions covering metadata, interface ptrs, error handling with/without config
   374|- ✅ **Suite: 84/0/1** (+1 pass)
   375|- ✅ **Plugins: 14%** (all 3 .so now real — 0 remaining stubs)
   376|- ◀ Committed: `2198e92dd`
   377|
   378|### Session 2026-05-24 (Kanban plugin real + test)
   379|
   380|- ✅ **Kanban plugin real** — `plugin_kanban.c` upgraded from stub to full in-memory board with task CRUD:
   381|  - 8 boards × 256 tasks per board in static storage
   382|  - Real task storage: description, column (todo/in_progress/done/blocked), priority (1-5), assignee, sticky flag
   383|  - JSON output with proper escaping via `json_escape()`
   384|  - Error handling: max boards, board full, null name, invalid board ID
   385|  - All 4 interface functions: create_board, add_task, get_board, list_boards
   386|  - Plugin version 1.0.0
   387|- ✅ **test_plugin_kanban.c** — 45 assertions covering create, list, add task (×3), get board, error paths, cleanup
   388|- ✅ **test_runner.sh** — kanban plugin test added
   389|- ◀ **Suite: 83/0/1** ✅ (+1 pass)
   390|- ◀ **Plugins: ~12%** (1 more real plugin, was 10%)
   391|- ◀ Committed: `ddede70b9`
   392|
   393|## Session 2026-05-22 (Current)
   394|
   395|- ✅ **G55: MCP SSE transport** — Replaced "not yet implemented" stub with real `connect_sse_server()`. Uses `mcp_server_set_sse()` + HTTP/SSE transport with auth header injection, root directories, tool filtering, and dynamic handler registration. Same pattern as stdio path.
   396|- ◀ **Suite: 82/0/1** ✅
   397|- ◀ Committed: `2216e98b4`
   398|
   399|- ✅ **Phase 113: Agent loop retry + fallback** — LLM call retry loop with exponential backoff (1s-16s). Wire `api_max_retries` (default 3), `fallback_model`, `fallback_providers` from config → llm_config_t. Fallback model first, then comma-separated fallback providers. Test: 9 new assertions.
   400|- ◀ **Suite: 82/0/1** (config test now 79 assertions)
   401|- ◀ Committed: `b667db689`
   402|
   403|- ✅ **M31: File tool test** — test_file.c, 35 assertions for file CRUD (read/write/search) + sandbox enforcement. Covers: create, overwrite, parent dirs, empty file, missing path, offset/limit read, non-existent file, null args, pattern/glob search, path traversal /etc blocking, 500B content, special path chars
   404|- ✅ **-Werror fixes in file.c** — `/*` within comment, unused `fread` return
   405|- ✅ **test_runner.sh updated** — file_tool (35 tests) added
   406|- ◀ **Tests now 28 files, 1,473 assertions. Suite: 59/59 pass, 0 fail, 3 skip**
   407|- ◀ Committed: `2ebd96df5`
   408|
   409|### Session 2026-05-22 (Later — L01: BSM Secrets Manager)
   410|- ✅ **L01: Bitwarden Secrets Manager** — New `secrets` subsystem with config struct (`secrets_config_t`), `include/hermes_secrets.h` header, `src/secrets.c` implementation
   411|  - Config: YAML keys `secrets.bitwarden.{enabled,access_token,organization_id,bws_path,install_timeout}`, env vars `HERMES_SECRETS_BITWARDEN_*`
   412|  - Full config wiring: defaults, YAML parsing, env override, diff, export, merge, validation
   413|  - Runtime: lazy bws install (curl download + chmod), BSM auth via `bws project list`, secret resolution `${BSM:name}` in config values
   414|- ◀ Committed: (current commit)
   415|
   416|### Session 2026-05-22 (Evening — L07: xAI Encrypted Reasoning Replay)
   417|- ✅ **L07: xAI encrypted reasoning replay** — Thread encrypted_content across conversation turns
   418|  - Added `encrypted_content` field to message_t, provider_response_t, llm_response_t
   419|  - Parse from xAI response messages (non-stream + stream path)
   420|  - Include in xAI request body for assistant messages on subsequent calls
   421|  - Generic llm_client parser also handles encrypted_content field
   422|  - Full message lifecycle: new, clone, free all handle encrypted_content
   423|  - Wire through agent_loop assistant message creation
   424|- ◀ Committed: `17a88748f`
   425|
   426|## Session 2026-05-22 (L03 + M30)
   427|
   428|- ✅ **L03: xAI Web Search** — search_xai() via Responses API + web_search tool dispatch
   429|- ✅ **M30: Web tool test** — test_web.c, 22 assertions for all 3 web handlers + 6 backend dispatch
   430|- ✅ **Bugfix: use-after-free in web_search_handler** — backend arg read after json_free(args) caused all backends to silently fall through to default searxng
   431|- ◀ **Tests: 36 files, 6,920 lines, 69 passed, 0 failed, 0 skipped**
   432|- ◀ Committed: `46b284ac1`
   433|- ◀ **~315 gaps remaining** (2 closed this session)
   434|
   435|### Session 2026-05-26 — TIRITH policy depth (O13)
   436|
   437|- ✅ **Policy rule engine** — 4 rule types (file_path, network, command, env_var), 3 actions (deny/allow/warn), glob matching via fnmatch, ALLOW-overrides-DENY semantics, structured results with rule name + reason
   438|- ✅ **YAML config loading** — `tirith_policy_load_yaml()` parses YAML rule definitions without external YAML dep, `tirith_policy_load_defaults()` loads 15 sensible built-in rules
   439|- ✅ **Global policy instance** — `tirith_policy_global_init()` called at CLI startup from config, `tirith_policy_global()` runtime accessor
   440|- ✅ **Config integration** — `security.tirith_policy_text` field (4096 chars) in security_config_t, parsed from YAML config, env var `HERMES_TIRITH_POLICY_TEXT`
   441|- ✅ **Type-specific eval** — `eval_paths()` extracts path tokens, `eval_network()` extracts URLs, `eval_command()` full command match, `eval_env()` extracts env var refs
   442|- ✅ **Full pipeline** — `eval_all()` runs all 4 type checks, returns worst verdict (DENY > WARN > allow)
   443|- ✅ **test_tirith_policy.c** — 57 assertions covering: lifecycle, rule matching, ALLOW-over-DENY, all 4 type evaluations, full pipeline, YAML loading (single/multiple rules), defaults (15 rules), edge cases (max rules 64, glob ?, null safety, reason strings)
   444|- ◀ **Build/doc: 86%→90%** (1/3 remaining O-section gaps closed: O13)
   445|- ◀ **Suite: 103/0/0** (+1 test, 57 assertions; was 102/0/0)
   446||- ◀ **Remaining O gaps:** O14 (sandbox escape detection), O02 (Windows build)
   447|
   448|### Session 2026-05-27 — Sandbox escape detection (O14)
   449|
   450|- ✅ **sandbox_escape.c** — New module with 48 escape patterns across 7 categories:
   451|  - Path traversal (`../`, `..%2f`, deep traversal)
   452|  - Sensitive paths (`/etc/passwd`, `/etc/shadow`, `/etc/ssh/`, `/root/.ssh/`)
   453|  - /proc/ and /sys/ access (`/proc/self/`, `/proc/1/`, `/proc/`, `/sys/`)
   454|  - Shell injection (`;`, backtick, `$()`, `$(<`)
   455|  - Escape commands (`bwrap`, `nsenter`, `chroot`, `docker run/exec`, `kubectl exec`, `flatpak`, `snap`)
   456|  - Environment poisoning (`LD_PRELOAD`, `LD_LIBRARY_PATH`, `PYTHONPATH`, `PERL5LIB`)
   457|  - Fork bombs (`:(){`, `fork()`, `forkbomb`)
   458|  - Reverse shells (`nc -e`, `bash -i >&`, `>/dev/tcp/`, `curl/wget pipe-to-sh`, `socat`, `ncat`)
   459|- ✅ **Wired into terminal_handler()** — blocks escape patterns before subprocess execution
   460|- ✅ **Wired into exec_code_handler()** — blocks escape patterns before code execution
   461|- ✅ **sandbox_escape_init()** called from tools_init_all()
   462|- ✅ **sandbox_escape_add_pattern()** for custom pattern registration
   463|- ✅ **test_sandbox_escape.c** — 14 tests covering all categories + lifecycle + null safety + blocked count tracking
   464|- ✅ **Bugfix: `&&`/`||` too aggressive** — removed from default patterns (common in legit commands)
   465|- ✅ **Bugfix: `${}` too aggressive** — removed (normal shell var expansion)
   466|- ◀ **Build/doc: 90%→95%** (O14 closed, O02 Windows remains)
   467|- ◀ **Suite: 106/0/0** (+1 test, 14 assertions; was 105/0/0)
   468|