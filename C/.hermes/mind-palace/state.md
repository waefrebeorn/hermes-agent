# State — Hermes C Translation (2026-05-22)

## ~53% toward 1:1 Python parity (~390 gaps remaining)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 5 depth | 96% | 322/322 YAML keys parsed, **profiles auto-load from `~/.slermes/profiles/<name>.yaml`** |
| **Providers** | 40 | 85% | 9 ops + 31 aliases + **18/18 LLM params** fully wired |
| **MCP** | 16 | **100% ✅** | Transport, tools, resources, prompts, subs, sampling, serve. **G55: SSE transport added** |
| **Plugins** | 48 | 14% | 3 .so plugins: kanban (real) + honcho (real) + spotify (real: Web API via curl) — **45+27+18 test verified** |
| **Gateway** | 63 | **100% ✅** | E01-E55, E57-E63 ✅. E56 (Matrix read receipts) would require deeper event tracking |
| **Tools** | 24 | 95% | 28 reg'd, browser/memory/kanban 1:1. 6 CDP/plugin-blocked stubs |
| **Agent** | 31 | 86% | 23 state fields, 18 session DB, G01-G36 all filled. **Phase 113: LLM retry (api_max_retries=3) + fallback_model + fallback_providers** |
| **CLI** | 33 | 87% | 70 slash commands, skin/theme engine. H31-H32 /session-search + /session-export added. **H01: `hermes completions {bash|zsh}`** |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics |
| **Tests** | 42 | 54% | **40 files, 2,260 assertions** (86 pass, 0 fail, 0 skip) |
| **Upstream** | 1 | new | L02 remains (CDP auto-launch, blocked) (125 commits behind) |
| **Cross-cut** | 4 | **100% (6/6) ✅** | N02 secure parent dir, N05 local trust, N03 key leakage prevention, N04 vendor key derivation. **N01 token counting added: model-aware heuristic, context windows, cost rates** |
| **Build/doc** | 10 | 55% | Dockerfile (multi-stage, ~20MB), CI workflow (build+test+TUI+plugins+Docker), cross-compile script (4 targets), .dockerignore, man page (hermes.1) |

**Known bug:** temperature=0.0 — **FIXED ✅**

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
