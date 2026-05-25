# Hermes C Translation — Battleship Roadmap v4

**DA v15 (2026-05-24) — Fresh code survey. No inherited gap counts. All gaps verified from source.**  
Old battleship-v3 had 500 gaps at 34% — most stubs were already resolved, numbers were stale.  
This is a complete reset with fresh counts.

**Real Parity (2026-05-27 code survey): ~63% — ~255 estimated gaps remaining**
  **UPDATE: CLI commands.c is actually 79 real cmds, NOT 197 stubs. S01/CDP already fixed.**
  **CLI command parity: 79 C cmds vs 66 Python cmds = ~119% (C has extras). Gap is MODULE depth (8 .c vs 88 .py files), not stub cmds.**

---

## Sector A: Core (12/16 done, ~4 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| A01 | agent_init.py (1638L) port | P0 | Complex Python SDK deps, plugin_llm integration |
| A01b | account_usage.py (326L) | **✅ Fixed** | Provider usage tracking. OpenRouter credits API. 11 tests. |
| A02 | agent_runtime_helpers.py (2189L) port | P0 | Runtime helpers for agent lifecycle |
| A03 | conversation_loop.py port | P0 | Core loop has C (agent_loop.c), but Python has richer interrupt/budget/checkpoint |
| A04 | conversation_compression.py port | P1 | Context compression strategy |
| A05 | async_utils.py port | P1 | Async helper utilities (low priority if loop is sync) |

**Verdict:** Core agent loop, LLM client, tool_guardrails, i18n, credential_pool, fallback_routing, checkpoint, context, system_prompt, vault, rate_limit all done ✅

---

## Sector B: Agent Modules (33/77 .c files, ~44 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| B01 | account_usage.py (326L) | **✅ Fixed** | Provider usage tracking. OpenRouter credits API. 11 tests. |
| B02 | agent_init.py (1638L) | P0 | Complex — see A01 |
| B03 | agent_runtime_helpers.py (2189L) | P1 | Runtime helper functions |
| B04 | anthropic_adapter.py | — | ✅ Part of provider_anthropic.c |
| B05 | async_utils.py | P2 | Async helpers |
| B06 | auxiliary_client.py (5289L) | P1 | **Large** — API client for external services |
| B07 | azure_identity_adapter.py | P2 | Azure identity (low priority) |
| B08 | background_review.py | P2 | Background review agent |
| B09 | browser_provider.py | P2 | Browser provider abstraction |
| B10 | browser_registry.py | P2 | Browser registry (partly in browser.c) |
| B11 | chat_completion_helpers.py | P1 | Streaming, stop reasons, content parsing |
| B12 | codex_responses_adapter.py | P2 | Codex mode adapter |
| B13 | codex_runtime.py | P2 | Codex runtime integration |
| B14 | context_compressor.py | P1 | Context window compression |
| B15 | context_engine.py (211L) | **✅ Fixed** | Pluggable context engine vtable interface. Default implementations for all methods. 11 tests. |
| B16 | context_references.py (518L) | **✅ Fixed** | File/URL context references. 46 tests. Suite: 216/0/0 |
| B17 | copilot_acp_client.py | P2 | Copilot ACP client |
| B18 | credential_sources.py | P1 | ✅ Partially covered by credential_pool.c |
| B19 | curator.py | P1 | Skill curation daemon |
| B20 | curator_backup.py | P2 | Curator backup mechanism |
| B21 | display.py | P1 | C has lib/libtui/ — check parity |
| B22 | gemini_cloudcode_adapter.py | P1 | Google Cloud Code adapter |
| B23 | gemini_native_adapter.py | P1 | Gemini native API adapter |
| B24 | google_code_assist.py | P2 | Google Code Assist integration |
| B25 | google_oauth.py | P2 | OAuth for Google services |
| B26 | image_gen_provider.py | P1 | ✅ Partially — image_gen.c handles FAL |
| B27 | image_gen_registry.py (145L) | **✅ Fixed** | Thread-safe registry. FAL provider default. 6 tests. |
| B28 | insights.py (930L) | P2 | Session analytics from DB |
| B29 | iteration_budget.py | P1 | ✅ Part of budget_tracker.c |
| B30 | memory_manager.py | P1 | ✅ memory.c + memory storage |
| B31 | memory_provider.py | P1 | Memory provider plugin interface |
| B32 | message_sanitization.py | P1 | ✅ Part of sanitize.c |
| B33 | model_metadata.py | P1 | ✅ provider_metadata.c covers this |
| B34 | models_dev.py | P2 | Dev model utilities |
| B35 | nous_rate_guard.py | P2 | ✅ Covered by rate_guard.c |
| B36 | plugin_llm.py | P1 | LLM via plugin system |
| B37 | process_bootstrap.py | P1 | Process bootstrap helpers |
| B38 | prompt_builder.py | P0 | **Important** — system prompt construction |
| B39 | rate_limit_tracker.py | — | ✅ Covered by libratelimit |
| B40 | shell_hooks.py (847L) | P1 | Shell hooks — depends on plugin system |
| B41 | skill_commands.py | P1 | ✅ skill_bundles.c covers this |
| B42 | stream_diag.py | P2 | Stream diagnostics |
| B43 | subdirectory_hints.py | — | ✅ Ported as subdir_hints.c |
| B44 | title_generator.py | P1 | ✅ title.c covers this |
| B45 | tool_executor.py | P1 | Tool execution orchestration |
| B46 | tool_result_classification.py | P1 | Result classification |
| B47 | video_gen_provider.py | — | ✅ Covered by video_gen.c |
| B48 | video_gen_registry.py | **✅ Fixed** | Thread-safe provider registry ported. FAL provider registered as default. 6 tests. |
| B49 | web_search_provider.py | — | ✅ Covered by web.c |
| B50 | web_search_registry.py | **✅ Fixed** | Capability-based web search/extract/crawl registry. Config + env + legacy pref walk. 10 tests. |

**True new ports needed:** ~35 (the ones marked P0-P1 without ✅)

---

## Sector C: CLI (8 .c files, ~88 Python modules, ~30 gap modules)
**UPDATE: 79 real cmd_ functions in commands.c (3702 lines). 0 true stubs.**
**"197 stub commands" in DA v15 was stale — refers to module depth, not commands.c stubs.**

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| C01 | Python hermes_cli/ directory | — | 88 .py files, only 8 .c files exist |
| C02 | prompt_toolkit autocomplete | P1 | C uses basic fgets() |
| C03 | Rich/panel display (banner, formatting) | P1 | C has ANSI colors, no Rich depth |
| C04 | Setup wizard (first-run flow) | P1 | C has no first-run wizard |
| C05 | Skin engine depth (data-driven theming) | P1 | ✅ Done — lib/libskin/ |
| C06 | Command autocomplete (CLI) | P1 | Missing in C |
| C07 | Session management UI | P1 | ✅ Done — sessions cmd real |
| C08 | Async event loop | P2 | Sync fgets() loop |
| C09 | Gateway subcommand | P1 | ✅ Done — gateway mode works |
| C10 | Plugin subcommand (list/install/remove) | P1 | C has basic plugin load |
| C11 | Config editing from CLI | P1 | ✅ cmd_config real |
| C12 | Logs viewer (follow, level filters) | P1 | Missing in C |
| C13 | Cron CLI (list/add/remove) | P1 | ✅ Done — cron is sector H |
| C14 | Profile management | P1 | ✅ Done — profile set/get/clear |
| C15 | One-shot mode | P1 | ✅ Done |
| C16 | Kanban CLI | P2 | ✅ Done — 10 kanban tools |
| C17 | Stub commands remediation (197/237 are stubs) | P0 | **Many CLI commands are printf stubs** |
| C18-C80 | Specific hermes_cli/*.py ports | P2+ | Remaining CLI modules |

**Verdict:** C CLI is 8 .c files vs Python's 88. Major gap. ~62 specific modules unported.

---

## Sector D: Tools (31 init functions, ~20 feature gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| D01 | feishu_doc_tool.py (138L) port | P2 | Feishu document tool |
| D02 | feishu_drive_tool.py (431L) port | P2 | Feishu drive tool |
| D03 | mixture_of_agents_tool.py (542L) port | P2 | MoA tool |
| D04 | microsoft_graph_auth.py (245L) port | P2 | MS Graph auth |
| D05 | microsoft_graph_client.py (408L) port | P2 | MS Graph client |
| D06 | managed_tool_gateway.py (167L) port | P2 | Gateway management from CLI |
| D07 | browser CDP tool registration wired to real handler | **✅ Fixed** | Real CDP WebSocket client (browser.c:1272), registered via browser_cdp_handler |
| D08 | environment backends (modal, daytona, singularity, vercel_sandbox, managed_modal, file_sync) | P2 | C has local/ssh/docker only |
| D09 | computer_use macOS CUA driver | P1 | X11/Wayland done, macOS CUA missing |
| D10 | transcribe tool — verify it's registered as a tool | **✅ Fixed** | lib/libtranscribe/ wired as registered tool. 5 tests. Suite: 217/0/0 |
| D11 | image_gen multi-provider (stable diffusion local) | P2 | FAL only |
| D12 | video_gen multi-provider | P2 | FAL only |
| D13 | voice_mode real-time streaming | P2 | Shell-based espeak only |
| D14 | MCP tool dynamic tool registration | P1 | ✅ Done in mcp_tool.c |
| D15 | OpenRouter client as tool | P2 | Python has tools/openrouter_client.py |
| D16 | neutron TTS (neutts_synth.py) | P2 | Python TTS plugin |
| D17 | yuanbao tools (yuanbao_tools.py) | P2 | ✅ Done in discord.c? No — yuanbao is in gateway |
| D18 | slash_confirm.py (CLI confirmation tool) | P2 | CLI approve/deny |
| D19 | mcp_oauth_manager.py | P2 | ✅ lib/libmcp_oauth/ exists |
| D20 | lazydeps (lazy_deps.py) | P2 | Dependency loader |

---

## Sector E: Gateway (19/31 platform modules, ~12 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| E01 | api_server.py port — REST API gateway | P1 | C gateway only talks to platforms, has no REST API |
| E02 | feishu_comment.py + rules port | P2 | Feishu comment support |
| E03 | signal_rate_limit.py port | P2 | Rate limiting for Signal |
| E04 | telegram_network.py port | P2 | Telegram network abstraction |
| E05 | wecom_callback.py port | P2 | WeCom callback handling |
| E06 | wecom_crypto.py port | P2 | WeCom crypto support |
| E07 | yuanbao_media.py port | P2 | Yuanbao media handling |
| E08 | yuanbao_proto.py port | P2 | Protobuf for Yuanbao |
| E09 | yuanbao_sticker.py port | P2 | Yuanbao stickers |
| E10 | base.py adapter helpers port | P1 | Shared platform base |
| E11 | helpers.py port | P1 | Platform helpers |
| E12 | Per-platform integration tests | P1 | 19 platforms, 0 tests |

---

## Sector F: MCP (1 .c file, ~2 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| F01 | MCP transport — HTTP/SSE | P1 | ✅ Done via http.c |
| F02 | MCP stdio transport | P1 | ✅ Done |
| F03 | MCP sampling/createMessage | P1 | ✅ Done |
| F04 | MCP resource subscriptions | P2 | Resource subscription lifecycle |

---

## Sector G: ACP (5/9 modules, ~4 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| G01 | acp_adapter/__main__.py | P2 | Entry point |
| G02 | acp_adapter/auth.py | P1 | ✅ lib/libmcp_oauth/ covers token exchange |
| G03 | acp_adapter/edit_approval.py | P1 | ✅ Done |
| G04 | acp_adapter/entry.py | P1 | ✅ Done (ACP server entry) |
| G05 | acp_adapter/events.py | P1 | ✅ Done |
| G06 | acp_adapter/permissions.py | P1 | ✅ Done |
| G07 | acp_adapter/session.py | P2 | Session management |
| G08 | acp_adapter/tools.py | P1 | ✅ Done (resource.c server.c) |
| G09 | acp_adapter/server.py | P1 | ✅ Done |

---

## Sector H: Cron (3/3 modules, 0 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| H01 | Public API (cron.h) | — | ✅ Done |
| H02 | Job lifecycle (cron_extras.c) | — | ✅ Done |
| H03 | Scheduler (cron_tools.c) | — | ✅ Done |

✅ **100% complete**

---

## Sector I: TUI (2 .c + lib, ~6 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| I01 | TUI fullscreen session browser | P1 | ✅ Done (DB-backed) |
| I02 | Response box wrapping | P1 | Text wrapping in TUI |
| I03 | Config editor (TUI) | P2 | Inline config editing |
| I04 | Theme engine | P2 | Skinning/theming system |
| I05 | Mobile/responsive mode | P3 | Small-screen layout |
| I06 | Image viewer (TUI) | P3 | Image display in terminal |

---

## Sector J: Plugins (10/16 dirs, ~6 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| J01 | browser plugin | P2 | ✅ browser.c + camofox |
| J02 | context_engine plugin | P2 | Python only |
| J03 | disk-cleanup plugin | P2 | ✅ C plugin exists |
| J04 | example-dashboard plugin | P3 | Python only |
| J05 | google_meet plugin | P2 | Python only |
| J06 | hermes-achievements plugin | P2 | ✅ C plugin exists |
| J07 | image_gen plugin | P2 | ✅ Done (FAL.ai) |
| J08 | kanban plugin | P1 | ✅ C kanban.c |
| J09 | memory plugin (honcho, mem0, supermemory) | P1 | ✅ memory.c + plugin interface |
| J10 | model-providers plugin | P1 | 28 provider dirs, 9 C native |
| J11 | observability plugin | P2 | ✅ C plugin exists |
| J12 | platforms plugin | P2 | ✅ discord in C |
| J13 | spotify plugin | P2 | ✅ C plugin exists |
| J14 | teams_pipeline plugin | P2 | Python only |
| J15 | video_gen plugin | P2 | ✅ C video_gen.c |
| J16 | web plugin | P2 | Python only |
| J17 | Plugin .so build target (10 targets, 0 built) | P1 | C plugins compile but .so not present |

---

## Sector K: Providers (9/28, ~19 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| K01 | alibaba provider plugin | P2 | |
| K02 | alibaba-coding-plan | P2 | |
| K03 | arcee | P2 | |
| K04 | azure-foundry | P2 | ✅ Azure partially covered by provider_azure.c |
| K05 | copilot | P2 | GitHub Copilot integration |
| K06 | copilot-acp | P2 | ACP via Copilot |
| K07 | gmi | P2 | Google Model Infrastructure |
| K08 | huggingface | P2 | HF inference |
| K09 | kilocode | P2 | |
| K10 | kimi-coding | P2 | |
| K11 | minimax | P2 | |
| K12 | nous | ✅ Fixed | Portal tags + reasoning defaults wired in provider_openai.c |
| K13 | novita | P2 | |
| K14 | nvidia | P2 | Nvidia NIM |
| K15 | ollama-cloud | P2 | |
| K16 | openai-codex | P2 | Codex mode |
| K17 | opencode-zen | P2 | |
| K18 | qwen-oauth | P2 | Qwen auth |
| K19 | stepfun | P2 | |
| K20 | xiaomi | P2 | |
| K21 | zai | P2 | |

---

## Sector L: Config (~322/432 keys, ~10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| L01 | Config key completeness audit | P1 | Current ~322 keys, need to verify against Python's 432 |
| L02 | TTS config (voice, provider) | P1 | ✅ Done |
| L03 | STT config | P2 | Speech-to-text config |
| L04 | Per-provider model config | P1 | ✅ Done |
| L05 | Display/skin config | P1 | ✅ Done |
| L06 | Security/guardrails config | P1 | ✅ Done |

---

## Sector M: Libraries (57 lib dirs, ~5 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| M01 | Library test coverage | P2 | ~20/57 libs have tests |
| M02 | Library API docs | P3 | Missing header docs |
| M03 | libncurses vendored lib cleanup | P3 | Large vendored lib |
| M04 | sqlite3 vendored cleanup | P3 | Vendored sqlite3 amalgamation |

---

## Sector N: Build/Doc (5 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| N01 | Full CI pipeline with test pass gate | P1 | Need test_runner.sh CI integration |
| N02 | Cross-compile target testing | P2 | Scripts exist but untested in CI |
| N03 | Root README rewrite with accurate numbers | P0 | Current README has stale stats |
| N04 | API docs generation (doxygen) | P2 | No doc generation |
| N05 | Vault documentation sweep | P1 | Vault docs need refresh |

---

## Sector O: Upstream Sync (variable)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| O01 | New Python modules since last sync | P1 | Check git log on Python side |
| O02 | Drift in provider API schemas | P1 | API changes in upstream |

---

## Sector P: Security (5 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| P01 | URL safety depth | P1 | ✅ Done — url_safety.c |
| P02 | Command allowlist | P1 | ✅ Done — terminal.c |
| P03 | Rate limiting (tool-level) | P1 | ✅ Done — rate_limit.c |
| P04 | File permission sandbox | P1 | ✅ Done — file_safety.c |
| P05 | MCP tool security audit | P1 | MCP tool security review |
| P06 | Plugin sandbox (seccomp/namespace) | P2 | Plugin sandboxing |
| P07 | Gateway input sanitization | P1 | ✅ Done — sanitize.c |

---

## Sector Q: CLI Depth (10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| Q01 | 197 stub commands to real implementations | P0 | Major CLI gap |
| Q02 | Autocomplete with tab-completion | ✅ Fixed | Tab completion via line_edit lib |
| Q03 | Line editing (history, C-r search) | ✅ Fixed | termios raw mode, arrow keys, history (100 entries) |
| Q04 | Rich terminal output (panels, tables) | P1 | ANSI colors only |
| Q05 | Multi-line paste support | P1 | Current single-line input |
| Q06 | Ctrl-C / interrupt handling | P1 | ✅ Done — lib/libinterrupt/ |
| Q07 | Scrollback buffer | P2 | Terminal scrollback |
| Q08 | Color theme support | P2 | Skin engine exists |
| Q09 | Mouse support (clickable links) | P3 | Terminal mouse |
| Q10 | File picker dialog | P3 | TUI file picker |

---

## Sector R: Provider Quirks (10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| R01 | Per-provider temperature=0 guard fix | P0 | Verified fixed across 9 providers |
| R02 | Streaming FIM for all providers | P1 | Fill-in-the-middle streaming |
| R03 | Vision support metadata per provider | P1 | Support_vision lookup |
| R04 | Reasoning effort config (deepseek, xai, anthropic) | P1 | ✅ Done |
| R05 | Response format enforcement per provider | P1 | ✅ Done |
| R06 | Token counting parity per provider | P1 | C tokenizer has heuristic counting |
| R07 | Provider-specific error mapping | P1 | ✅ error_classifier.c |
| R08 | Rate limit backoff per provider | P1 | ✅ Done |
| R09 | Streaming delta format per provider | P1 | ✅ Done |
| R10 | Max tokens / context window enforcement | P1 | ✅ Done |

---

## Sector S: Stubs (0 true stubs)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| S01 | browser_cdp tool registration → real CDP handler | ✅ Fixed | Registered to browser_cdp_handler (L1491-1494), stub_cdp_handler is dead code |

---

## Sector T: Tests (20 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
|| T01 | Test suite performance (173 files timeout at 120s) | **P0** | ✅ **Fixed** — all 175 blocks parallelized, 210/0/0 in <60s |
| T02 | Gateway per-platform tests (19 platforms, 0) | P1 | Integration tests |
| T03 | Provider adapter tests (9 providers, per-API) | P1 | Mock API tests |
| T04 | CLI command tests (237 cmd_ functions) | P1 | Dispatch tests |
| T05 | Plugin sandbox tests | P1 | Security boundary |
| T06 | Memory leak CI (valgrind) | P1 | Long-running stability |
| T07 | Fuzz testing for JSON/config/parser | P2 | Input robustness |
| T08 | MCP/ACP protocol integration tests | P2 | Client-server |
| T09 | TUI component tests | P2 | Complex UI |
| T10 | Cross-compile build tests | P2 | Portability |
| T11 | Thread safety tests | P2 | Race conditions |
| T12 | Benchmark regression harness | P3 | Performance |
| T13 | Test coverage tracking (gcov/lcov) | P2 | Coverage metrics |
| T14 | End-to-end inference tests | P1 | Full pipeline: config → LLM → response |
| T15 | Environment backend tests (docker/ssh) | P2 | Backend-specific |
| T16 | Browser CDP integration test | P2 | Needs CDP server |
| T17 | Library unit test coverage (57 libs, ~20 with tests) | P2 | Library tests |
| T18 | Test ordering/dependency audit | P2 | Test isolation |
| T19 | CI pipeline test gate | P1 | Tests must pass before merge |
| T20 | Test timeout configuration | P1 | 120s default too short |

---

## Sector U: CI/CD (5 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| U01 | Cross-compile matrix in CI | P2 | aarch64, arm |
| U02 | Release automation (semver + changelog) | P2 | Tagging/release |
| U03 | Code coverage upload (Codecov) | P2 | Metrics |
| U04 | CVE scanning (dependency vulns) | P2 | Supply chain |
| U05 | Performance regression gate | P3 | Binary size/startup |

---

## Summary Table

| Sector | Gaps | Priority | Verdict |
|--------|------|----------|---------|
| A: Core | ~4 | P0 | 75% done |
| B: Agent | ~44 | P0 | 57% (44/77 files) |
| C: CLI | ~30 | P0 | 79 real cmds, module depth gap |
| D: Tools | ~20 | P1 | Most tools real, feature gaps |
| E: Gateway | ~12 | P1 | 61% coverage |
| F: MCP | ~2 | P1 | Mostly done |
| G: ACP | ~4 | P1 | 56% done |
| H: Cron | 0 | — | **100%** ✅ |
| I: TUI | ~6 | P1 | 25% |
| J: Plugins | ~6 | P1 | 63% |
| K: Providers | ~18 | P1 | 32% **K12 fixed** (portal tags + reasoning) |
| L: Config | ~10 | P1 | 75% |
| M: Libraries | ~5 | P2 | Good |
| N: Build/Doc | ~5 | P1 | Mixed |
| O: Upstream | ~2 | P1 | Ongoing |
| P: Security | ~7 | P1 | 70% |
| Q: CLI Depth | ~8 | P0 | **Q02/Q03 fixed** |
| R: Provider Quirks | ~10 | P1 | 60% |
| S: Stubs | 0 | — | **All fixed** ✅ |
| T: Tests | ~19 | P1 | **T01 fixed** (210/0/0 in <60s) |
| U: CI/CD | ~5 | P2 | Basic infra works |
| **Total** | **~179** | — | **T01, Q02, Q03, K12 fixed** |

*Generated 2026-05-24 from fresh code survey. See da-audit-v15.md for verification methodology.*
