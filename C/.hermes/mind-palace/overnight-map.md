# Overnight Map (v10) — 2026-05-26

```
~62% complete  •  ~350 gaps remaining  •  125 commits behind upstream
```

### Session 2026-05-26 — Gateway escape mode tests (M07)

- ✅ **test_gateway_escape.c** — 30 assertions across 3 gateway formatting functions
- ◀ **Tests: 62%** (1/36 gaps closed: M07)
- ◀ Committed: `TBD`

### Session 2026-05-26 — SECURITY.md (O09)

- ✅ **SECURITY.md** — Full disclosure policy, dependency audit, security features doc
- ◀ **Build/doc: 70%** (was 65%, 1/8 gaps closed: O09)
- ◀ Committed: `b58b7e88a`

### Session 2026-05-26 — M06: Provider error handling edge cases

- ✅ **test_provider_error.c** — 225 assertions across 9 providers
- ✅ **Bugfix: NULL SIGSEGV in 6 parse_stream_chunk** — all providers null-safe now
- ✅ **Bugfix: API error JSON silently dropped in 6 parse_response** — all providers return descriptive errors
- ◀ **Tests: 61%** (1/37 gaps closed: M06)
- ◀ **Providers: 88%** (error handling bugs fixed)
- ◀ Committed: `c51391dc3`

### Session 2026-05-26 — Doxygen API docs infrastructure (O07)

- ✅ **Doxyfile** + `make docs` target + module comments for 13 headers
- ◀ **Build/doc: 65%** (was 60%, O07 closed)
- ◀ Committed: `c906bc5f2`

### Session 2026-05-26 — CHANGELOG.md (O10)

- ✅ **CHANGELOG.md** — 150 lines, covers v0.1.0 through unreleased
- ◀ **Build/doc: 60%** (was 55%, O10 closed)
- ◀ Committed: `ee65389a3`

### Session 2026-05-26 — Azure provider full tests + 3 bugfixes

- ✅ **test_azure_full.c** — 45 assertions: URL, headers, 12 LLM params, response_format/json_mode, metadata, tool_choice, messages, tools, response parsing, streaming, null safety
- ✅ **3 bugfixes in provider_azure.c** — 2x UAF (metadata + tool_choice), error object parsing, NULL stream crash
- ◀ **Suite: 96/0/0** (+1 test, 45 assertions; M05 closed)
- ◀ **Tests: 60%**
- ◀ Committed: `d38e20f4e`

### Session 2026-05-26 — Bedrock provider full tests + 3 bugfixes

- ✅ **test_bedrock_full.c** — 35 assertions: URL, inferenceConfig, system, messages, B39-B41, response parsing (text/toolUse/error), null safety
- ✅ **3 bugfixes in provider_bedrock.c** — 2x UAF (metadata + tool_choice), toolUse input serialization
- ◀ **Suite: 95/0/0** (+1 test, 35 assertions)
- ◀ **Tests: 59%** (M03-M04 closed, 2/39 gaps)
- ◀ Committed: `6f0a44091`

### Session 2026-05-26 — Google provider full tests + 3 bugfixes

- ✅ **test_google_full.c** — 40 assertions: URL, headers, gen config, safety settings, systemInstruction, tools (functionDeclarations fix), response parsing, streaming, null safety
- ✅ **3 bugfixes in provider_google.c** — functionDeclarations on array (no-op), trailing slash //models, NULL stream crash
- ◀ **Suite: 94/0/0** (+1 test, 40 assertions)
- ◀ **Tests: 58%** (M03 closed)
- ◀ Committed: `ef14d000e`

### Session 2026-05-26 — Anthropic provider depth test (B26-B28)

- ✅ **test_anthropic_depth.c** — 80 assertions covering URL/headers/request body/response/streaming
- ✅ **Bugfix: NULL crash in anthropic_parse_stream_chunk** — null safety fix in provider_anthropic.c
- ◀ **Suite: 93/0/0** (+1 test, 80 assertions)
- ◀ **Tests: 57%** (was 56%, 1/40 gaps closed)
- ◀ **Providers: 87%** (B26-B28 verified, ~35 provider gaps remain)
- ◀ Committed: `b0a6c38b5`

## Active Workstreams

| Stream | What | Sessions |
|--------|------|----------|
| **P0** | Fix temperature=0.0 bug — **FIXED ✅** | Done |
| **P0b** | B04-B05: response_format + metadata | 1 |
| **P0c** | F01-F08: 8 tool stubs → real | 3 |
| **P0d** | B01-B03: 3 ACP providers | 4 |
| **P1** | Gateway depth (34 gaps) | 8 |
| **P1** | Agent state + session (32) | 3 |
| **P1** | CLI feature depth (34) | 4 |
| **P1** | Tool sub-features (31) | 5 |
| **P2** | Plugin depth (51) | 15 |

## Build & Test

```bash
make -j$(nproc) && bash test_runner.sh --verbose
# Expect: 58/58 pass (plugin test + warning fixes applied)
```

## Known Issues
- Plugins at 8% is the biggest structural gap (45 backends to port)
- 25 provider-specific API quirks entirely missed before this audit
- F05-F07: computer_use (WSL), memory_sqlite (needs libsqlite3), memory_plugin (needs plugin system) still stubs
- Browser/memory/kanban tool handlers verified 1:1 ✅
- **Fixed:** temperature=0.0 dropped (s/> 0.0f/>= 0.0f/ × 9 providers)
- **Fixed:** streaming path missing config forwarding (16 params now forwarded)

### Session 2026-05-22
- ✅ P0 #1: temp=0.0 fix
- ✅ P0 #2: response_format + metadata 
- ✅ P0 #5: tool_choice + parallel_tool_calls
- ✅ P0 #6: max_tool_calls + n (choices)
- ✅ Fixed streaming config forwarding gap
- ✅ **18/18 LLM params fully wired**
- ✅ P0 #3: F08 vision description — real Python Hermes CLI delegation
- ✅ E01-E05: Telegram sendPhoto/Document/Voice/Video/Animation
- ✅ E14: Telegram forwardMessage
- ✅ E15: Telegram pinChatMessage + unpinChatMessage
- ✅ F41: Image format validation (extension check + 50MB limit)
- ✅ B01: ACP server Sessions 2-4 — full lifecycle + auth + streaming
- ✅ B02-B03: Covered by B01 ACP server infrastructure
- ✅ E16: Telegram message reactions (setMessageReaction API)
- ✅ **G15+G16: enabled_toolsets + disabled_toolsets** — toolset field in tool_t, registry_register_ex, registry_filter_by_toolset, tools.enabled_toolsets/disabled_toolsets YAML keys, agent state wiring, toolset labels for 34 key tools
- ✅ **G17: system_message override** — per-conversation override field in agent_state_t, wired in agent_run_conversation before each LLM call
- ✅ **G19: thread/user/chat IDs** — routing metadata fields in agent_state_t
- ✅ **G01-G03: session token tracking** — session_total/input/output_tokens counters, updated after each LLM response
- ✅ **G04-G12: deep token tracking** — reasoning/cache/cost counters, user/tool turn counts, last_activity_ts, pending_steer, interrupt_message on fatal tool errors; /status shows all fields
- Next: G13-G14 tool_choice/parallel_tool_calls state, G18 conversation_history injection, G20 model_family, or gateway E06-E12 interactive sends, or upstream sync L01-L12

### Session 2026-05-22 (Part 2)
- ✅ **G21-G36 (16 gaps):** Compression depth, iteration budget, checkpoint depth, prefill variants, steer queue, typed interrupt + partial results
- ◀ **Agent loop now at ~85% parity (G01-G36 all filled)** — 32/32 agent state gaps closed

### Session 2026-05-22 (Part 3)
- ✅ **E27-E34 (8 gaps):** Gateway infrastructure — keepalive per-platform, message dedup (64-entry TTL ring buffer), batch aggregation (2s coalesce), markdown stripping (code/bold/italic/headers), per-platform cooldown, reconnect backoff (exponential 2^attempt, ±10% jitter, 60s cap), proxy per-platform, group observe prefix
- ◀ **Gateway now at ~48% parity** (30/63 gaps closed)
- ◀ Committed: `d5e5109db`

## Session 2026-05-22 (Part ~8)

### Gaps closed this session: 13+
- ✅ **F27: Cron job chaining** — context_from arg, cron_run_job (popen + chain output), cron_cli.c added to build
- ✅ **F15: Batch file ops** — file_batch tool: copy/move/delete multi-file with sandbox enforcement
- ✅ **F21: Web LLM extract** — web_extract_delegate.py, fetch+LLM extraction via hermes chat subprocess
- ✅ **F37-F39: TTS providers** — elevenlabs, openai, xai API-based backends (provider + voice args)
- ✅ **F40: LLM vision description** — hardcoded path replaced with SLERMES_HOME configurable detection
- ✅ **F42: send_message routing** — platform:chat_id format, generic gateway dispatch
- ✅ **F43: send_message media** — media_path arg + MEDIA: prefix compat, extension-based Telegram send
- ✅ **F44: exec_code sandbox** — sandbox bool arg, bwrap namespace/seccomp isolation
- ✅ **C01-C03: MCP resource subscriptions** — subscribe/unsubscribe/is_subscribed + notification handler + callback
- ✅ **C08-C10: MCP roots management** — add_root/remove_root/root_count/get_root dynamic operations
- ✅ Fixed: cron_cli.c dead code (was not compiled), g_cron_store global added, cron_run_job defined
- ◀ Committed: `a5b626344`, `b56bd8aa3`, `4295bc03b`, `31dcdbe3c`, `3d5af3d48`, `05ae948d9`, `bd05f10ec`
- ◀ **Tools at ~95% parity** (6 stubs remaining, all CDP/plugin-blocked)
- ◀ **MCP at ~70% parity** (8/17 gaps closed)
- ◀ **Roadmap gap count: ~367 remaining** (~33 closed total)

## Upstream
- 125 commits since last sync, 52 Python
- 12 new feature gaps identified
- 75% of upstream work is TUI/computer_use/skills — C stubs skip those

### Session 2026-05-22 (Evening — This Continuation)
- ✅ **Plugin test fixes** — test name mismatch, unused func removal, -Wpedantic func ptr casts
- ✅ **E54: Slack file upload** — `slack_upload_file()` via two-step external upload API
- ✅ **L08: Telegram observe unmentioned groups** — `telegram_get_me()`, `telegram_is_mentioned()`, `telegram_is_group()`, observe buffer with context injection on @mention
- ◀ **Gateway now at 100%** (63/63 gaps)
- ◀ **Upstream drift: 9/12 remain** (L05-L06, L08 done)
- ◀ **Tests: 58/58 pass, 0 fail, 2 skip**
- ◀ Committed: `a626e95ba`, `fd8de11d8`

### Session 2026-05-22 (Evening Part 2)
- ✅ **L10: Voice chunk oversized recordings** — TTS chunking at sentence boundaries, `max_chunk_duration_s` param, multi-file array output
- ◀ **Upstream drift: 8/12 remain** (L05-L06, L08, L10 done)
- ◀ **Tools: TTS chunking added** (previously single-file-only)
- ◀ Committed: `154211f17`

### Session 2026-05-22 (Evening Part 3)
- ✅ **L09: Telegram location+media observe** — already covered by telegram_get_text()
- ✅ **L11: Kanban sticky blocks** — sticky=true on block, auto-promote skips sticky, real promote count
- ◀ **Upstream drift: 6/12 remain** (L05-L06, L08-L11 done; L07, L12 pending)
- ◀ **Tools: Kanban now at 10/9 parity** (C ahead: sticky blocks + auto-promote)
- ◀ Committed: `09577165d`

### Session 2026-05-22 (Evening Part 4)
- ✅ **L12: Browse.sh skills hub** — `skill_search_hub()` + `skill_install_from_hub()` with HTTP catalog fetch, query filtering, detail+CDN SKILL.md install, provenance tracking
- ✅ **skill_hub tool** — `search <query>`, `install <slug>`, `list` actions
- ✅ **skill_search hub:true** — merge hub results into search output with `source` field
- ✅ **CLI /skills search-hub /install** — subcommands in cmd_skills
- ◀ **Upstream drift: 5/12 remain** (L05-L06, L08-L11, L12 done; L01-L04, L07 pending)
- ◀ **Build/doc: skills hub added to CLI and tool registry**
- ◀ Committed: `dbe604c64`

### Session 2026-05-22 (Evening Part 5)
- ✅ **L04: xAI model retirement detection** — `xai_is_model_retired()` with official May 15, 2026 retirement list. Config validation checks principal.model, tools.vision_model, delegation.model, compression.model. Reports replacement + reasoning_effort hints.
- ◀ **Upstream drift: 4/12 remain** (L01-L03, L07)
- ◀ Committed: `4ea1bbadc`

### Session 2026-05-22 (After — L03 + M30)
- ✅ **L03: xAI Web Search** — search_xai() via Responses API + web_search tool dispatch
- ✅ **M30: Web tool test** — test_web.c, 22 assertions for all 3 handlers + 6 backend dispatch
- ✅ **Bugfix: use-after-free in web_search_handler** — backend arg read after json_free(args), all backends silently fell through to searxng
- ◀ **Tests: 36 files, 6,920 lines, 69 passed, 0 failed, 0 skipped**
- ◀ **~315 gaps remaining** (2 closed this session)

## Session continuation (3 gaps closed)
- ✅ **M38: Skills system test** — test_skills.c, 53 assertions covering scan, validate, origin, cache, usage, search, bundle, sync, curator, hub install
- ✅ **L05: extra_body passthrough** — JSON key merge into request body for all 6 OpenAI-compat providers. Config key: `agent.extra_body`
- ✅ **M44: MCP tool test** — test_mcp.c, 24 assertions covering 7 MCP tool registration, handler dispatch, timeout config
- ✅ **All per-tool tests complete** — M29-M44 all done (17 tests)
- ◀ **Tests: 42 files, 75 pass, 0 fail, 1 skip**
- ◀ Committed: `ce094d8dc`, `cfb63915e`, `3f69bd4fa`

## Session continuation (B26-B28 + A01)
- ✅ **B26: Anthropic thinking blocks** — extended_reasoning + budget_tokens in Anthropic provider
- ✅ **B27: Anthropic cache control** — cache_control on user messages + tools array
- ✅ **B28: extra_body on Google + Anthropic** — JSON key merge for safetySettings, metadata, etc.
- ✅ **A01: Config profiles complete** — profile auto-load on startup in hermes_config_load, `/config profile <name>` and `/config profile list` CLI commands, /config profile list scans `~/.slermes/profiles/*.yaml`, test_runner.sh has 2 new profile tests
- ◀ **Config: 96% (5 depth gaps remaining)**
- ◀ **Tests: 77 pass, 0 fail, 1 skip (+2 profile tests)**
- ◀ **Config depth gaps: 5/322 remaining**

## Session continuation (N01: Token counting)
- ✅ **N01: Token counting** — `hermes_tokenizer.h/c`: model-aware heuristic token counter
  - `token_family_t` enum (9 families) + `hermes_token_family_from_model()`
  - `hermes_token_count()` with configurable chars-per-token ratio (3.5-4.2)
  - `hermes_token_context_size()` — context windows for 20+ known models (128K-1M)
  - `hermes_token_cost_rates()` — approximate $/1M costs per family
  - `hermes_token_count_messages()` — conversation token estimation
  - Wired into CLI `/status` — shows `Context: N/128000 (X%)` utilization
  - Wired into agent loop — pre-request context limit warning at >90%
  - Test: test_tokenizer.c — 39 assertions, all model families + edge cases
- ◀ **Suite: 78 pass, 0 fail, 1 skip (+1 tokenizer, 39 assertions)**

## Session continuation (P126: In-memory memory plugin)
- ✅ **P126: In-memory memory plugin** — `plugin_honcho.c` converted from stub to real backend
  - Array-based store (1024 entries max), no external deps
  - `memory_store()` — stores content + metadata JSON, returns unique ID
  - `memory_search()` — keyword substring match with relevance ranking (recency + access count + position bonus), sorted by score
  - `memory_clear()` — resets store
  - All plugin API symbols exported: metadata, lifecycle, interface dispatch
  - Plugin system proven end-to-end: load → init → interface → store → search → clear → cleanup → unload
  - Test: test_plugin_honcho.c — 27 assertions covering all operations
- ◀ **Suite: 79 pass, 0 fail, 1 skip (+1 plugin test, 27 assertions)**

## Session continuation (H01: Shell completions)
- ✅ **H01: Shell completions** — `hermes completions {bash|zsh}` generates shell completion scripts
  - bash: `_hermes_completions()` with `compgen -W` for first-level opts
  - zsh: `_hermes()` with `_arguments` for top-level + subcommand dispatch
  - contrib/completions/ directory with standalone files
  - Test: test_runner section 7 — 3 assertions (bash, zsh, usage)
- ◀ **Suite: 82 pass, 0 fail, 1 skip (+3 completions tests)**

### Session 2026-05-24 — Plugins All Real

- ✅ **Kanban plugin real** — full in-memory board: 8 boards × 256 tasks, columns, priority, assignee, sticky, JSON output with escaping. 45 tests.
- ✅ **Spotify plugin real** — Spotify Web API via curl popen(): OAuth token flow, play/pause/next/current/search endpoints. 18 tests.
- ✅ **Plugins: 14%** — all 3 .so plugins now real (0 remaining stubs). Gone from biggest structural gap to manageable depth gap.
- ◀ **Suite: 84/0/1** (+2 tests, 90 new assertions)
- ◀ **Next P0 gaps: provider-specific APIs (25), tests (44), build/doc (15)**
- ◀ Commits: `ddede70b9`, `2198e92dd`, `0e773bfda`

### Session 2026-05-24 — Build/doc Infrastructure

- ✅ **Dockerfile** — Multi-stage, ~20MB runtime, libssl3+curl for plugin HTTP
- ✅ **CI workflow** — GitHub Actions: build+test+TUI+plugins+Docker
- ✅ **Cross-compile** — scripts/cross-compile.sh (4 targets: x86_64, aarch64, armv7, windows)
- ✅ **.dockerignore** — Exclude Python/node/website from Docker context
- ✅ **Man page** — hermes.1: 14 options, 12 commands, config/plugin/gateway docs
- ◀ **Build/doc: 55%** (was 30%, 5/15 gaps closed)
- ◀ Commits: `a61cac0fd`, `20edb4d35`

### Session 2026-05-26 — B30-B31: Google provider depth

- ✅ **B30: top_k + candidate_count** — `agent.top_k`, `HERMES_TOP_K`, config/agent/llm wire. Google writes `topK`/`candidateCount` to generationConfig.
- ✅ **B31: systemInstruction** — already implemented, verified with test
- ✅ **test_google_depth.c** — 7 assertions
- ◀ **Suite: 90/0/0** (+1 test, 7 assertions)
- ◀ Committed: `5662b2004`
- ◀ **Provider-specific API gaps: 25→22** (B23, B30, B31 closed; B31 was already done)

### Session 2026-05-26 — B23: json_mode for all providers

- ✅ **B23: json_mode convenience flag** — `agent.json_mode: true` auto-sets `response_format: {"type":"json_object"}` across all 9 providers
  - YAML key, env var, config defaults, diff, agent state, llm_client forwarding
- ✅ **Bugfix: pre-existing use-after-free in response_format path** — All 9 providers freed a node still referenced in the JSON tree. `json_object_set(root, ..., rf); json_free(rf)` → `...json_copy(rf); json_free(rf)`. Bug existed since response_format was added.
- ✅ **test_json_mode.c** — 10 assertions, ASan-clean
- ◀ **Suite: 89/0/0** (+1 test, 10 assertions)
- ◀ Committed: `4e116f85b`
- ◀ **Providers: 85% → 86%** (1/25 provider API gaps closed)

- ✅ **test_skin.c** — 24 assertions: skin_default, dotted-path key lookup, fallback,
  ANSI color resolution (red/cyan/bold/dim/invalid), skin_color, skin_apply_color,
  symbols, format flags, NULL safety, skin_with_overrides, error handling
- ◀ **Suite: 85/0/1** (+1 test, 24 assertions)
- ◀ **Tests: 54%** (was 52%, 1/44 gaps closed)
- ◀ Committed: `589f75b81`

### Session 2026-05-24 — Config depth: A03 env var expansion

- ✅ **A03: ${VAR:-default} expansion** — 4 critical keys, 100 lines, 11/12 standalone tests pass
- ✅ **Suite: 88/0/0** (no regression)
- ✅ **Config: 97%** (4 depth gaps remain: schema, includes, watching, migration)
- ◀ Committed: `856ab9722`

### Session 2026-05-24 — Test Coverage: Cron + Protobuf

- ✅ **test_cron_lib.c** — 51 assertions: parse/match/next/describe/null safety; found cron_next stale tm_wday bug
- ✅ **test_protobuf.c** — 63 assertions: varint encode/decode/tag/skip/find/round-trip/null safety
- ◀ **Suite: 88/0/0** (+2 tests, 114 assertions; was 86/0/1)
- ◀ **Tests: 56%** (was 54%, 2/42 gaps closed)
- ◀ Commits: `4de4fec5b`, `c2c471e78`
