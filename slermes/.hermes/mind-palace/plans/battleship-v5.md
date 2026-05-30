# Hermes C Translation — Battleship Roadmap v5

**Triple DA audit (2026-05-28) — Fresh code survey + stub hunt. ~400 estimated gaps.**
**No inherited gap counts from prior audits. Every gap verified from source code.**

**ERRATA (v18 — 2026-05-28):** S01-S03 (CDP stubs) incorrectly classified. All 13 browser tools have real handlers — `browser_get_images_handler` at browser.c:931 (HTML parse), `browser_vision_handler` at 1301 (CDP screenshot), `browser_console_handler` at 1346 (CDP console), `browser_dialog_handler` at 1383 (CDP dialog), `browser_cdp_handler` at 1273 (CDP raw command). `stub_cdp_handler` at browser.c:1172 is dead code (no tool registers to it). S01-S03 removed. Remaining stubs renumbered S01-S09 (0 🔴, 2 🟡, 7 🔵). Total gaps: ~397 → ~394.

**Verified State:**
- Suite: 217/0/0 (180 test files) | CLI: 79 real cmds | Tools: 70 registered
- Gateway: 19 platforms | Providers: 10 C modules | Libs: 58 units
- Agent .c files: ~46 | Test .c files: 182 | Source .c files: 148

**Upstream sync:** 133 new upstream commits since last sync. 3 new Python files (ntfy platform, skills_ast_audit.py). Rest are bugfixes.

**Real Parity: ~60% — ~400 gaps remaining**

---

## Sector A: Core Agent (4/16 done, ~12 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| A01 | agent_init.py (1638L) port | P0 | Complex Python SDK deps, plugin_llm integration |
| A02 | agent_runtime_helpers.py (2189L) port | P0 | Runtime helpers for agent lifecycle |
| A03 | conversation_loop.py port depth | P0 | Core loop in agent_loop.c, Python has richer interrupt/budget/checkpoint |
| A04 | conversation_compression.py port | P1 | C has core compress (llm_client.c), Python has token-budget tail, scaled budget |
| A05 | async_utils.py port | P1 | Sync bypass fine, but utility functions still needed |
| A06 | streaming diagnostics (stream_diag.py, 280L) | P1 | Per-attempt counters, exception chains, retry logging |
| A07 | chat_completion_helpers.py depth (2097L) | P1 | Streaming, stop reasons, content parsing depth |
| A08 | model_metadata.py depth (1828L) | P1 | C has provider_metadata.c, Python has richer model DB |
| A09 | models_dev.py (726L) | P2 | Dev model utilities, cache, capabilities |
| A10 | background_review.py (587L) | P2 | Background review agent |
| A11 | title_generator.py depth | P1 | C has title.c, Python has richer heuristics |
| A12 | conversation_loop.py: partial-stream continuation | P1 | Upstream fix: finish_reason=length handling |

## Sector B: Agent Modules (46/78 .c files, ~55 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| B01 | context_compressor.py: scaled summary budget | P1 | Proportional to compressed content (20%) |
| B02 | context_compressor.py: token-budget tail | P1 | Budget-based tail (not fixed count) |
| B03 | context_compressor.py: anti-thrashing failure cooldown | P1 | 600s cooldown on FAILURE (success cooldown done) |
| B04 | auxiliary_client.py (5289L) port | P1 | **Massive** — API client for external services |
| B05 | curator.py (1781L) — skill curation daemon | P1 | Depends on skill_bundles (done) |
| B06 | curator_backup.py (696L) | P2 | Backup mechanism |
| B07 | shell_hooks.py (847L) — needs C hook registry | P1 | Config-driven subprocess callbacks |
| B08 | insights.py (930L) — session analytics depth | P1 | C has basic /insights, Python has richer: per-platform/skill/date |
| B09 | credential_sources.py (448L) | P1 | C has credential_pool.c, Python has richer sources (Bitwarden) |
| B10 | memory_manager.py (609L) | P1 | C has memory.c, Python has vector storage, expiry |
| B11 | memory_provider.py (279L) | P1 | Plugin memory provider interface |
| B12 | skill_commands.py (523L) — rewrite for C | P1 | C has skill_bundles.c, Python has hub/install/remove |
| B13 | plugin_llm.py (1046L) | P1 | LLM via plugin system |
| B14 | process_bootstrap.py (167L) | P1 | Tool process bootstrap helpers |
| B15 | tool_executor.py (910L) — concurrent exec depth | P1 | C has sequential/concurrent in agent_loop, Python has ThreadPoolExecutor |
| B16 | tool_result_classification.py depth | P1 | C has tool_result.c, Python has richer classification |
| B17 | anthropic_adapter.py (2244L) — provider depth | P1 | C has provider_anthropic.c (basic), needs streaming, tool_use, caching |
| B18 | bedrock_adapter.py (1289L) | P1 | AWS Bedrock provider |
| B19 | gemini_native_adapter.py (971L) | P1 | Google Gemini native API |
| B20 | gemini_cloudcode_adapter.py (909L) | P1 | Google Cloud Code adapter |
| B21 | google_oauth.py (1059L) | P1 | OAuth for Google services |
| B22 | google_code_assist.py (452L) | P2 | Google Code Assist |
| B23 | azure_identity_adapter.py (555L) | P2 | Azure identity |
| B24 | codex_responses_adapter.py (1082L) | P2 | Codex response format |
| B25 | codex_runtime.py (448L) | P2 | Codex runtime |
| B26 | copilot_acp_client.py (686L) | P2 | Copilot ACP client |
| B27 | image_gen_provider.py depth (242L) | P1 | C has FAL path, needs SD/local multi-provider |
| B28 | video_gen_provider.py depth (299L) | P2 | Multi-provider (FAL only) |
| B29 | web_search_provider.py depth (221L) | P1 | C has web.c, needs multi-search-engine support |
| B30 | browser_registry.py (223L) | P2 | Browser provider abstraction |
| B31 | browser_provider.py (175L) | P2 | Browser provider interface |
| B32 | rate_limit_tracker.py depth (246L) | P1 | C has libratelimit, Python has richer tracking |
| B33 | nous_rate_guard.py depth (325L) | P1 | C has librateguard, Python has richer features |
| B34 | message_sanitization.py depth (444L) | P1 | C has sanitize.c, needs threat detection patterns |
| B35 | prompt_builder.py depth (1465L) | P0 | C has system_prompt.c, Python has richer assembly |
| B36 | prompt_caching.py depth | P1 | C has prompt_caching.c, Python has richer cache strategies |
| B37 | provider_anthropic.c: empty content fallback fix | P1 | Returns "" for streaming content — missing actual content extraction |
| B38 | provider: streaming depth (all 10 providers) | P1 | llm_client.c marked "synchronous for simplicity" |
| B39 | image_routing.c: per-provider vision lookup | P1 | TODO: per-model supports_vision from config |
| B40 | account_usage.c: Codex usage fetch | P1 | TODO: port `_fetch_codex_account_usage` |
| B41 | display.py depth (cute messages, spinner) | P1 | C has lib/libtui/, needs kawaii spinner parity |
| B42 | trajectory.c depth | P2 | Incomplete scratchpad detection |
| B43 | vault.c depth | P2 | Vault features |
| B44 | file_safety.py depth | P1 | C has file_safety.c, Python has richer threat scanning |
| B45 | secret_sources/bitwarden.py port | P2 | Bitwarden secret source (new upstream) |
| B46 | context_engine.py depth | P1 | C has context_engine.c (11 tests), Python has richer engine |

## Sector C: CLI (8 .c vs 88 Python .py, ~45 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| C01 | auth CLI — login/logout/setup (hermes_cli/auth.py 7601L) | P0 | **Massive** — OAuth flows, token management |
| C02 | gateway CLI — start/stop/status (gateway.py) | P1 | Gateway management commands |
| C03 | portal CLI — model/provider portal (portal_cli.py) | P1 | Portal commands |
| C04 | secrets CLI — manage API keys (secrets_cli.py) | P1 | Key management |
| C05 | setup wizard — first-run config (setup.py) | P1 | Interactive first-run setup |
| C06 | webhook CLI — webhook management (webhook.py) | P2 | Webhook config |
| C07 | kanban CLI — kanban board management (kanban.py) | P1 | C has kanban tools, needs CLI frontend |
| C08 | kanban_db CLI (kanban_db.py) | P1 | DB-backed kanban |
| C09 | skills hub CLI — install/remove/search | P1 | Hub integration |
| C10 | tools_config CLI — tool configuration | P1 | Tool config management |
| C11 | gateway_windows CLI | P2 | Windows-specific gateway |
| C12 | oneshot CLI commands | P1 | Quick one-shot query mode |
| C13 | fallback CLI commands | P2 | Fallback when config is missing |
| C14 | env_loader CLI | P1 | Environment variable management |
| C15 | web_server CLI | P2 | Embedded web server |
| C16 | _parser CLI | P2 | Config parser |
| C17 | plugins CLI — plugin management | P1 | Install/remove/list plugins |
| C18 | portal_tags CLI — portal tag management | P1 | Tag management |
| C19 | config CLI — config editing from CLI | P1 | Config set/get/edit |
| C20 | fallback_config CLI | P2 | Fallback config when YAML missing |
| C21 | main CLI — entry point depth | P1 | CLI entry point features |
| C22 | help/build — /help formatting, build info | P1 | Help system depth |
| C23 | shell completion — bash/zsh completions | P1 | Shell completion scripts |

## Sector D: Tools (44 .c files, ~30 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| D01 | browser_get_images — switch to real CDP handler | P1 | Currently routes to stub_cdp_handler |
| D02 | browser_vision — switch to real CDP handler | P1 | Currently routes to stub_cdp_handler |
| D03 | browser_console — switch to real CDP handler | P1 | Currently routes to stub_cdp_handler |
| D04 | Feishu doc tool port (feishu_doc_tool.py, 138L) | P2 | Feishu document integration |
| D05 | Feishu drive tool port (feishu_drive_tool.py, 431L) | P2 | Feishu drive integration |
| D06 | OpenRouter client tool (openrouter_client.py, 33L) | P2 | OpenRouter model routing |
| D07 | Neutron TTS (neutts_synth.py, 104L) | P2 | Neutron TTS backend |
| D08 | Computer use macOS CUA driver | P1 | X11/Wayland done, macOS CUA missing |
| D09 | Computer use Windows support | P2 | Windows CUA driver |
| D10 | Environment backends (modal, daytona, vercel) | P2 | C has local/ssh/docker only |
| D11 | Slash confirm tool (slash_confirm.py, 167L) | P2 | Gateway-side confirmation |
| D12 | Clarify gateway (clarify_gateway.py, 278L) | P2 | Clarify tool for gateways |
| D13 | Tool config CLI (tools_config.py) | P1 | Tool config management |
| D14 | Tool output limits (tool_output_limits.py, 92L) | P1 | Configurable truncation limits |
| D15 | Tool result storage depth (result_storage.c) | P1 | Turn budget, file persistence |
| D16 | Skills AST audit (skills_ast_audit.py) | P1 | New upstream — skill AST validation |
| D17 | MS Graph auth (microsoft_graph_auth.py, 245L) | P2 | Microsoft Graph auth |
| D18 | MS Graph client (microsoft_graph_client.py, 408L) | P2 | Microsoft Graph client |
| D19 | Tool backend helpers (tool_backend_helpers.py, 144L) | P2 | Nous vendor gateway helpers |
| D20 | image_gen multi-provider (local SD) | P2 | FAL only, no local SD |
| D21 | video_gen multi-provider | P2 | FAL only |
| D22 | voice_mode real-time streaming | P2 | Shell-based espeak only |
| D23 | patch.c: fuzzy match fallback | P1 | Currently skips fuzzy match ("skip for now") |
| D24 | skills.c: hub resync on install | P1 | Marked but not triggered |
| D25 | Web search multi-provider | P1 | web.c has basic, needs multi-engine |
| D26 | ntfy gateway tool | P2 | New upstream platform |

## Sector E: Gateway (19/31 platforms, ~25 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| E01 | api_server.py port — REST API gateway | P1 | C gateway only talks to platforms |
| E02 | ntfy platform port | P2 | New upstream — push notifications |
| E03 | feishu_comment.py + rules port | P2 | Feishu comment support |
| E04 | signal_rate_limit.py port | P2 | Rate limiting for Signal |
| E05 | telegram_network.py port | P2 | Telegram network abstraction |
| E06 | wecom_callback.py port | P2 | WeCom callback handling |
| E07 | wecom_crypto.py port | P2 | WeCom crypto support |
| E08 | yuanbao_media.py port | P2 | Yuanbao media handling |
| E09 | yuanbao_proto.py port | P2 | Protobuf for Yuanbao |
| E10 | yuanbao_sticker.py port | P2 | Yuanbao stickers |
| E11 | base.py adapter helpers port | P1 | Shared platform base |
| E12 | helpers.py port | P1 | Platform helpers |
| E13 | Platform shutdown no-op fix (server.c) | P1 | `plat.shutdown = NULL` — needs real cleanup |
| E14 | Per-platform integration tests | P1 | 19 platforms, 0 integration tests |
| E15 | Gateway input validation depth | P1 | Security hardening upstream |
| E16 | Webhook signature validation depth | P1 | Svix sig verification (upstream fix) |
| E17 | Feishu webhook auth | P1 | Auth secret requirements (upstream fix) |
| E18 | Discord role allowlist auth | P1 | Discord auth hardening (upstream fix) |

## Sector F: MCP (1 .c file, ~8 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| F01 | Sampling/createMessage handler | P1 | Not wired to real LLM call |
| F02 | Roots list support | P1 | MCP roots protocol |
| F03 | Resources templates | P2 | Resource template declarations |
| F04 | Prompts system | P2 | MCP prompt management |
| F05 | Streaming transport | P1 | SSE streaming from MCP server |
| F06 | MCP auth/oauth depth | P1 | lib/libmcp_oauth/ has basic |
| F07 | Client-side MCP (connect to external MCP) | P1 | Currently only serves MCP |

## Sector G: ACP (5 .c files, ~7 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| G01 | Sampling/chat completions via ACP | P1 | ACP can see edits, not run chat |
| G02 | File diff streaming | P1 | Streaming file diffs |
| G03 | Edit approval notification | P1 | User-facing confirm/deny |
| G04 | ACP server broadcasting | P2 | Server broadcasts state changes |
| G05 | ACP client SDK | P2 | Python has richer client API |

## Sector H: Cron (2 .c files, ~3 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| H01 | Cron: gateway notification delivery | P1 | TODO: send via gateway to channel |
| H02 | Cron: job persistence | P1 | SQLite-backed cron state |
| H03 | Cron: timing precision | P2 | Sub-minute cron scheduling |

## Sector I: TUI (src/tui/, ~10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| I01 | Config editor (TUI) | P2 | Inline config editing |
| I02 | Theme engine / skin config integration | P2 | Skinning/theming system |
| I03 | Image viewer | P3 | Image display in terminal |
| I04 | Mobile/responsive mode | P3 | Small-screen layout |
| I05 | Session list: DB-backed (no placeholder) | P1 | Currently shows placeholder when no DB |
| I06 | Tool activity feed depth | P1 | TUI tool activity tracking |
| I07 | Ncurses TUI build integration | P2 | `make tui` target reliability |

## Sector J: Plugins (10 .c, 17 plugin dirs, ~20 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| J01 | Plugin .so build system | P1 | 10 targets, 0 .so built |
| J02 | Kanban plugin | P2 | Python has kanban plugin |
| J03 | Memory plugins | P2 | honcho, mem0, supermemory adapters |
| J04 | Context engine plugins | P2 | Context engine plugin adapters |
| J05 | Disk cleanup plugin | P2 | Disk management |
| J06 | Achievements plugin | P2 | Gamification |
| J07 | Observability plugin | P2 | Metrics/traces |
| J08 | Example dashboard plugin | P2 | Sample UI |
| J09 | Google Meet plugin | P2 | Meeting integration |
| J10 | Spotify plugin | P2 | Music controls |
| J11 | Strike freedom cockpit | P2 | Payment integration |
| J12 | ntfy platform plugin | P2 | New upstream |
| J13 | Plugin sandbox (seccomp) | P2 | Security isolation |
| J14 | Plugin auto-discovery | P2 | Scan dirs for .so files |

## Sector K: Providers (10/29 done, ~25 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| K01 | Alibaba provider plugin | P2 | |
| K02 | Alibaba-coding-plan | P2 | |
| K03 | Arcee | P2 | |
| K04 | Azure-foundry | P2 | |
| K05 | Copilot | P2 | GitHub Copilot integration |
| K06 | GMI | P2 | Google Model Infrastructure |
| K07 | HuggingFace | P2 | HF inference |
| K08 | Kilocode | P2 | |
| K09 | Kimi-coding | P2 | |
| K10 | MiniMax | P2 | |
| K11 | Novita | P2 | |
| K12 | NVIDIA NIM | P2 | |
| K13 | Ollama-cloud | P2 | |
| K14 | OpenAI-codex | P2 | Codex mode |
| K15 | OpenCode-zen | P2 | |
| K16 | Qwen-oauth | P2 | |
| K17 | Stepfun | P2 | |
| K18 | Xiaomi | P2 | |
| K19 | Z.AI | P2 | |

## Sector L: Config (~322 keys vs Python ~432, ~15 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| L01 | Config key completeness audit | P1 | Verify all 432 Python keys have C equivalents |
| L02 | agent.compress_cooldown_secs | P1 | Anti-thrashing config (hardcoded 30s) |
| L03 | tool_output.max_bytes/ max_lines/max_line_length | P1 | Configurable tool output limits |
| L04 | agent.skill_search_paths | P1 | Custom skill directory paths |
| L05 | agent.tool_use_guard_level | P1 | Guardrail config depth |
| L06 | agent.image_input_mode | P1 | Image mode configuration |
| L07 | agent.reasoning_effort per-provider | P1 | Per-provider reasoning |
| L08 | agent.quiet_mode integration | P1 | CLI quiet mode |
| L09 | tools.enabled_ toolsets/disabled_ toolsets depth | P1 | Toolset filtering |
| L10 | agent.budget (token/cost) depth | P1 | Budget config |
| L11 | agent.session_db config | P1 | Session DB settings |
| L12 | agent.plugin_paths | P1 | Plugin loading paths |

## Sector R: Provider API Quirks (~15 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| R01 | temperature=0.0 guard fix: all 9 providers | P0 | Verified fixed, but re-check on new providers |
| R02 | Anthropic: streaming content extraction | P1 | Currently returns "" placeholder |
| R03 | Anthropic: tool_use content blocks | P1 | Anthropic-native tool format |
| R04 | Anthropic: prompt caching | P1 | Anthropic prompt caching headers |
| R05 | OpenAI: response_format strict mode | P1 | Structured output |
| R06 | OpenAI: service tier | P2 | Default/flex tier |
| R07 | Google: context caching | P1 | Gemini context caching |
| R08 | Google: safety settings | P2 | Content filtering |
| R09 | DeepSeek: FIM (Fill-in-Middle) | P1 | Code completion |
| R10 | DeepSeek: reasoning_effort | P1 | Reasoning tokens |
| R11 | xAI: reasoning_effort | P1 | Grok reasoning |
| R12 | Azure: API version management | P1 | Azure API versions |
| R13 | Bedrock: cross-region inference | P2 | Multi-region Bedrock |
| R14 | Provider response format: structured content | P1 | Rich content parsing (images, tool calls) |

## Sector S: Stubs (Verified from source — ~9 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| S01 | computer_use macOS CUA driver | P1 | X11/Wayland real, macOS returns error |
| S02 | computer_use Windows driver | P2 | Missing Windows backend |
| S03 | provider_anthropic.c: streaming content extraction | P1 | Returns "" for text content in certain paths |
| S04 | llm_client.c: synchronous streaming | P1 | "Currently synchronous for simplicity" — needs async streaming |
| S05 | account_usage.c: Codex fetch | P1 | TODO: `_fetch_codex_account_usage` |
| S06 | image_routing.c: vision capability lookup | P1 | TODO: per-model supports_vision from config |
| S07 | gateway/server.c: platform shutdown | P1 | `plat.shutdown = NULL` no-op |
| S08 | tui_fullscreen.c: placeholder session list | P2 | Shows placeholder when no DB connected |
| S09 | patch.c: fuzzy match skip | P1 | "Blind spot — skip fuzzy for now" |

## Sector T: Tests (182 .c vs 1152 Python .py, ~55 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| T01 | Agent API provider integration tests | P1 | Test each provider's API call path |
| T02 | Gateway platform integration tests | P1 | 19 platforms, 0 integration tests |
| T03 | CLI command tests | P1 | 79 commands, ~10 tested |
| T04 | Tool handler tests | P1 | 70 tools, most untested |
| T05 | MCP server integration tests | P1 | MCP protocol tests |
| T06 | ACP protocol tests | P1 | ACP message format |
| T07 | Cron scheduler tests | P1 | Cron job lifecycle |
| T08 | TUI component tests | P2 | TUI rendering tests |
| T09 | Plugin loading tests | P2 | .so plugin interface |
| T10 | Config parsing tests | P1 | YAML config edge cases |
| T11 | Gateway message format tests | P1 | Platform message serialization |
| T12 | Provider streaming tests | P1 | Streaming response handling |
| T13 | Provider tool call tests | P1 | Tool call/result format |
| T14 | Provider vision tests | P1 | Image input handling |
| T15 | Memory leak / ASan tests | P1 | Memory safety |
| T16 | Fuzz testing for input validation | P2 | Fuzz JSON/HTTP inputs |
| T17 | Session DB CRUD tests | P1 | SQLite session operations |
| T18 | Compression integration tests | P1 | Anti-thrashing, tool truncation, tail config |
| T19 | Migration tests (config upgrades) | P1 | Schema migration handling |
| T20 | Cross-platform build tests | P2 | Linux/macOS/Windows |

## Sector U: Upstream Sync (~10 gaps)

| ID | Gap | Priority | Notes |
|----|-----|----------|-------|
| U01 | ntfy platform port | P2 | New upstream — push notifications |
| U02 | skills_ast_audit.py port | P1 | New upstream — skill AST validation |
| U03 | Upstream security fixes — webhook signature | P1 | Svix webhook sig validation |
| U04 | Upstream security — Feishu auth | P1 | Feishu webhook auth hardening |
| U05 | Upstream security — Discord role allowlist | P1 | Discord auth hardening |
| U06 | Upstream security — dashboard loopback | P1 | Dashboard websocket restriction |
| U07 | Upstream streaming fixes — finish_reason=length | P1 | Partial-stream continuation |
| U08 | Upstream fix — response_transformed flag | P1 | Plugin-transformed response handling |
| U09 | Upstream fix — ACP final_response | P1 | ACP response delivery after streaming |
| U10 | Upstream Bitwarden secret source | P2 | EU Cloud + self-hosted server URL support |

---

## Summary

| Sector | Gaps | Priority | Status |
|--------|------|----------|--------|
| A: Core Agent | ~12 | P0-P1 | 25% done |
| B: Agent Modules | ~55 | P0-P2 | 59% agent .c files |
| C: CLI | ~45 | P0-P2 | 8 .c vs 88 .py |
| D: Tools | ~30 | P1-P2 | 70 registered tools |
| E: Gateway | ~25 | P1-P2 | 19/31 platforms |
| F: MCP | ~8 | P1 | 1 .c file |
| G: ACP | ~7 | P1 | 5 .c files |
| H: Cron | ~3 | P1 | 2 .c files |
| I: TUI | ~10 | P1-P3 | Partial |
| J: Plugins | ~20 | P1-P2 | 10 .c, 0 .so |
| K: Providers | ~25 | P2 | 10/29 |
| L: Config | ~15 | P1 | ~322/432 keys |
| R: API Quirks | ~15 | P0-P2 | Per-provider gaps |
| S: Stubs | ~12 | P1 | Verified from source |
| T: Tests | ~55 | P1 | 182 vs 1152 |
| U: Upstream | ~10 | P1-P2 | 133 new commits |
| **Total** | **~394** | | **~60% real parity** |

Generated 2026-05-28 from Triple DA audit: fresh code survey + stub hunt + upstream sync + upstream diff.
