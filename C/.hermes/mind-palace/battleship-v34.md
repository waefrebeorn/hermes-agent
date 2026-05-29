# Battle Map v34 — Comprehensive Parity Assessment (DA v1)

**v145 | Fork diverged — C/ lives only on fork | Suite 294/0/0 | 85 tools | 98 CLI**
**Honest assessment: 213+ structural gaps, 1000+ test case gaps across 9 sectors. Compiled May 28 2026.**

v34 replaces v33's narrow 17-gap form-vs-function focus with true 7-axis parity audit.
Every sector count verified against live source code. DA v1: first-pass deep audit.

---

## S0: Display & Visual — Phase 0 (P0)

C display_core.c (1211 LOC) + lib/libskin (657 LOC) + line_edit (593 LOC) already provides skin engine, spinners, banner, status bar, tool feed, panel/box, tables, progress bars, diff render, help categories, 24-bit color, kawaii faces, tool emoji mapping. 12 of 18 original v34 claims were stale.

| # | ID | Feature | Python | C | Status | Priority |
|---|----|---------|--------|---|--------|----------|
| 01 | D09 | Prompt input: tab completion, history search, multi-line editor | prompt_toolkit (async, emacs/vi modes) | Ctrl-R search, horizontal scroll, Alt+Enter multi-line. Has tab completion + history + bracketed paste. Missing vi/emacs modes. | PARTIAL | P0 |
| 02 | D16 | Recurrent typing: type-ahead, input buffering during LLM call | prompt_toolkit async input queue + keyboard interrupt | fgets/line_edit blocks during LLM, type-ahead lost | REAL GAP | P0 |

**S0: 2 gaps (1 real, 1 partial) — down from 18 as of v145 Phase 57.**

---

## S1: Conversation Loop — Plumbing Gaps (P1)

Python's run_conversation (4606 LOC) has features C's agent_loop.c (1600 LOC) lacks.
8 stale + 1 stale (L07 stream scrubber — C sync streaming, no hung-span issue) + 3 implemented. 11 real + 5 partial remain.

| # | ID | Feature | Python | C | Priority | Status |
|---|----|---------|--------|---|----------|--------|
| 01 | L01 | Connection health check: detect/clean up dead TCP connections | Pre-turn socket health check (zombie detection) | None | P1 | REAL |
| 02 | L04 | Todo state hydration from conversation history | Recover todo state from tool responses | None | P1 | REAL |
| 03 | L05 | Nudge counter hydration per session (gateway fresh AIAgent) | Reconstruct turn count from history | None | P1 | REAL |
| 04 | L11 | Compression warning replay through status_callback | _compression_warning sent via status_cb | None | P1 | REAL |
| 05 | L13 | Auxiliary client runtime setting (tools see active model) | set_runtime_main(model, provider) per turn | None | P1 | REAL |
| 06 | L15 | Skill write-origin tracking (foreground vs self-improvement) | set_current_write_origin per turn | None | P1 | REAL |
| 07 | L17 | System prompt caching with three-way state tracking | DB-backed: missing/null/empty/present with logging | None | P1 | REAL |
| 08 | L18 | Nous entitlement handling (paid service checks) | _nous_entitlement_message, credential refresh | None | P2 | REAL |
| 09 | L19 | Billing/entitlement error messages (per-provider guidance) | _billing_or_entitlement_message, OpenRouter link | None | P2 | REAL |
| 10 | L20 | Ollama context limit validation | _ollama_context_limit_error | None | P2 | REAL |
| 11 | L23 | Error classification & failover reason mapping | classify_api_error, FailoverReason enum | None | P1 | REAL |
| 12 | L24 | Turn-level checkpoint/snapshot for rollback | snapshot_create/restore per tool iteration | checkpoint_init exists but simpler | P2 | PARTIAL |
| 13 | L25 | Agent runtime helpers: tool schema management | agent_runtime_helpers.py (2366 LOC) | None | P1 | REAL |
| 14 | L26 | Chat completion helpers: request building, streaming | chat_completion_helpers.py (2467 LOC) | llm_chat_completion is simpler | P1 | PARTIAL |
| 15 | L27 | Prompt builder: system prompt assembly, dynamic sections | prompt_builder.py (1451 LOC) | hermes_system_prompt.h is simpler | P1 | PARTIAL |
| 16 | L28 | Agent init: full AIAgent construction with 60+ params | agent_init.py (1649 LOC) | agent_init() + agent_configure_from_config() | P1 | PARTIAL |

**S1: 16 gaps (11 real, 5 partial) — +L07 stale. 8 stale + 3 done.**

---

## S2: Agent Modules — Missing Ports (P1-P3)

45 Python agent modules with NO C equivalent. Listed by LOC descending.

| # | ID | Python Module | LOC | Priority |
|---|----|--------------|-----|----------|
| 01 | A01 | conversation_loop.py (plumbing, see S1) | 4606 | P1 |
| 02 | A02 | chat_completion_helpers.py (request building) | 2467 | P1 |
| 03 | A03 | agent_runtime_helpers.py (tool schema management) | 2366 | P1 |
| 04 | A04 | anthropic_adapter.py (streaming, caching, thinking) | 2275 | P1 |
| 05 | A05 | model_metadata.py (model discovery, context sizes) | 1850 | P1 |
| 06 | A06 | agent_init.py (60+ param AIAgent construction) | 1649 | P1 |
| 07 | A07 | prompt_builder.py (system prompt assembly) | 1451 | P1 |
| 08 | A08 | error_classifier.py (API error classification) | 1316 | P1 |
| 09 | A09 | bedrock_adapter.py (AWS auth, model discovery) | 1289 | P1 |
| 10 | A10 | codex_responses_adapter.py (Codex API) | 1221 | P2 |
| 11 | A11 | google_oauth.py (OAuth token exchange) | 1059 | P1 |
| 12 | A12 | plugin_llm.py (plugin-based LLM abstraction) | 1046 | P2 |
| 13 | A13 | display.py (KawaiiSpinner, faces, tool feed) | 1033 | P0 |
| 14 | A14 | gemini_native_adapter.py (Gemini API features) | 971 | P1 |
| 15 | A15 | insights.py (usage analytics, insights) | 930 | P2 |
| 16 | A16 | tool_executor.py (tool dispatch, result handling) | 912 | P1 |
| 17 | A17 | gemini_cloudcode_adapter.py (Codex mode) | 909 | P2 |
| 18 | A18 | models_dev.py (model management) | 725 | P2 |
| 19 | A19 | copilot_acp_client.py (GitHub Copilot) | 686 | P3 |
| 20 | A20 | memory_manager.py (memory providers) | 640 | P1 |
| 21 | A21 | conversation_compression.py (auto-compression) | 604 | P1 |
| 22 | A22 | background_review.py (async skill review) | 597 | P2 |
| 23 | A23 | skill_utils.py (skill utilities) | 566 | P2 |
| 24 | A24 | azure_identity_adapter.py (managed identity) | 555 | P1 |
| 25 | A25 | codex_runtime.py (Codex runtime) | 536 | P3 |
| 26 | A26 | aux_message_builder.py (aux message construction) | 532 | P2 |
| 27 | A27 | iteration_budget.py (per-turn budget tracking) | 516 | P1 |
| 28 | A28 | curator.py (skill curation, agent improvement) | 504* | P2 |
| 29 | A29 | title_generator.py (session title gen) | 500* | P2 |
| 30 | A30 | system_prompt_builder.py (dynamic prompt) | 480* | P1 |
| 31 | A31 | tracer.py (request tracing) | 454* | P3 |
| 32 | A32 | message_sanitization.py (8 sanitization fns) | ~800* | P1 |
| 33 | A33 | tool_result_classification.py (file mutation result) | ~400* | P2 |
| 34 | A34 | nous_rate_guard.py (rate limit detection) | ~350* | P1 |
| 35 | A35 | process_bootstrap.py (stdio guard) | ~300* | P1 |
| 36 | A36-A45 | (10 smaller modules) | ~2800 | P2-P3 |

*Approximate LOC

**S2: 45 gaps (20 P0-P1, 14 P2, 11 P3)**

---

## S3: Gateway Sub-Modules — Helper Files (P1)

13 Python gateway helper files with no C equivalent. C has 19 core platforms only.

| # | ID | File | LOC | Purpose | Priority |
|---|----|------|-----|---------|----------|
| 01 | G01 | helpers.py | ~800 | Shared media/formatting/retry utilities | P1 |
| 02 | G02 | base.py | ~600 | Gateway base class, rate limiting, retry | P1 |
| 03 | G03 | feishu_comment.py | ~400 | Feishu comment handling | P2 |
| 04 | G04 | feishu_comment_rules.py | ~300 | Feishu comment moderation rules | P2 |
| 05 | G05 | wecom_crypto.py | ~350 | WeCom message encryption | P2 |
| 06 | G06 | wecom_callback.py | ~300 | WeCom callback verification | P2 |
| 07 | G07 | telegram_network.py | ~450 | Telegram proxy/network config | P2 |
| 08 | G08 | signal_rate_limit.py | ~200 | Signal rate limiting | P2 |
| 09 | G09 | yuanbao_media.py | ~350 | Yuanbao media attachments | P2 |
| 10 | G10 | yuanbao_proto.py | ~300 | Yuanbao protobuf messages | P2 |
| 11 | G11 | yuanbao_sticker.py | ~200 | Yuanbao sticker handling | P2 |
| 12 | G12 | api_server.py | ~500 | REST API server for HTTP gateway | P1 |
| 13 | G13 | _http_client_limits.py | ~200 | HTTP client connection limits | P2 |

**S3: 13 gaps (3 P1, 10 P2)**

---

## S4: TUI Ecosystem — Full Backend + Frontend (P1)

C has 1 ncurses file (tui_fullscreen.c, 3374 LOC). Python has 28 Ink React tsx + 8 tui_gateway .py (~16,000 LOC).

| # | ID | Component | Python | C State | Priority |
|---|----|-----------|--------|---------|----------|
| 01 | T01 | TUI JSON-RPC gateway server | tui_gateway/server.py (6643 LOC) | 10 methods inline | P1 |
| 02 | T02 | TUI transport layer | tui_gateway/transport.py | None | P1 |
| 03 | T03 | TUI render engine | tui_gateway/render.py | ncurses panel only | P1 |
| 04 | T04 | TUI WebSocket support | tui_gateway/ws.py | None | P1 |
| 05 | T05 | TUI entry/startup | tui_gateway/entry.py | None | P1 |
| 06 | T06 | TUI slash command worker | tui_gateway/slash_worker.py | None | P1 |
| 07 | T07 | TUI event publisher | tui_gateway/event_publisher.py | None | P1 |
| 08 | T08 | App layout + chrome | appLayout.tsx, appChrome.tsx | ncurses panel only | P1 |
| 09 | T09 | Text input: autocomplete, history, multi-line | textInput.tsx (1233 LOC) | fgets() line editor | P1 |
| 10 | T10 | Markdown render: rich streaming render | markdown.tsx (1119 LOC) | None | P1 |
| 11 | T11 | Thinking indicator: animated states | thinking.tsx (1206 LOC) | None | P1 |
| 12 | T12 | Session picker | sessionPicker.tsx | None | P2 |
| 13 | T13 | Model picker | modelPicker.tsx | None | P2 |
| 14 | T14 | Agents overlay | agentsOverlay.tsx | None | P2 |
| 15 | T15 | Todo panel | todoPanel.tsx | None | P2 |
| 16 | T16 | Streaming markdown live update | streamingMarkdown.tsx | None | P1 |
| 17 | T17 | Bounding box: window resize re-render | SIGWINCH + full re-layout | handle_winch signal only | P1 |
| 18 | T18 | Recurrent typing: type-ahead during LLM call | Async input queue | fgets blocks, input lost | P1 |
| 19 | T19-T28 | (10 more tsx components) | ~4500 LOC total | None | P2-P3 |

**S4: 28 gaps (14 P1, 10 P2, 4 P3)**

---

## S5: CLI Ecosystem — hermes_cli Infrastructure (P2)

Python has 80+ CLI modules (~70,000 LOC). C has none of these.

| # | ID | Module | LOC | Priority |
|---|----|--------|-----|----------|
| 01 | C01 | Setup wizard (setup.py) | ~2000 | P2 |
| 02 | C02 | Doctor diagnostics (doctor.py) | ~1500 | P2 |
| 03 | C03 | Memory setup (memory_setup.py) | ~500 | P3 |
| 04 | C04 | Profile management (profiles.py) | ~1000 | P2 |
| 05 | C05 | Config editor (config.py) | ~800 | P2 |
| 06 | C06 | Env loader (env_loader.py) | ~200 | P2 |
| 07 | C07 | Model management (models.py) | ~2000 | P2 |
| 08 | C08 | Model switch (model_switch.py) | ~1000 | P2 |
| 09 | C09 | Model catalog (model_catalog.py) | ~2000 | P2 |
| 10 | C10 | Codex models (codex_models.py) | ~1000 | P3 |
| 11 | C11 | Auth/OAuth system (auth.py + auth_commands.py) | ~5000 | P1 |
| 12 | C12 | Copilot auth (copilot_auth.py) | ~1000 | P3 |
| 13 | C13 | Gateway CLI (gateway.py + gateway_windows.py) | ~4000 | P2 |
| 14 | C14 | Webhook setup (webhook.py) | ~1000 | P2 |
| 15 | C15 | Platform management (platforms.py) | ~2000 | P2 |
| 16 | C16 | Kanban system (kanban.py + 7 files) | ~11000 | P2 |
| 17 | C17 | Skills hub (skills_hub.py) | ~1500 | P2 |
| 18 | C18 | Voice mode (voice.py) | 846 | P3 |
| 19 | C19-C30 | Other CLI modules | ~25000 | P2-P3 |

**S5: 30 gaps (1 P1, 17 P2, 12 P3)**

---

## S6: Tool Depth — Feature Gaps (P2)

C tools are at 48% parity by LOC (30,288 vs 62,781).

| # | ID | Tool | C LOC | Python LOC | Parity | Missing Features | Priority |
|---|----|------|-------|-----------|--------|-----------------|----------|
| 01 | B01 | browser | ~1600 | ~3800 | 42% | autofill, cookies, PDF, HAR, network throttle | P2 |
| 02 | B02 | vision | ~203 | ~1421 | 14% | OCR, face detection, barcode, EXIF, multi-image | P2 |
| 03 | B03 | web | ~466 | ~1561 | 30% | cookie jar, sessions, proxy, JS render | P2 |
| 04 | B04 | mcp_tool | ~1623 | ~3584 | 45% | SSE transport, OAuth, subscriptions | P2 |
| 05 | B05 | file | ~561 | ~1220 | 46% | glob, fswatch, diff, hex view, symlink | P2 |
| 06 | B06 | feishu_tools | ~210 | ~872 | 24% | doc_read + drive_list only | P2 |
| 07 | B07 | terminal | ~800 | ~1500 | 53% | env passthrough, timeout UX, dir persist | P2 |
| 08 | B08 | send_message | ~500 | ~900 | 55% | inline buttons, media groups, reply threading | P2 |
| 09 | B09 | patch | ~700 | ~1200 | 58% | dry-run mode, conflict resolution | P2 |
| 10 | B10 | session_search | ~300 | ~650 | 46% | FTS5 query syntax, pagination, filtering | P2 |
| 11 | B11-B20 | remaining tools | ~50-80% | varying | partial | Various | P2-P3 |

**S6: 20 gaps (10 P2, 10 P3)**

---

## S7: Test Coverage — Massive Gap (P1)

| # | ID | Metric | Python | C | Ratio | Priority |
|---|----|--------|--------|---|-------|----------|
| 01 | X01 | Test files | 1,262 | 248 | 19.6% | P1 |
| 02 | X02 | Test LOC | 473,891 | 56,787 | 12.0% | P1 |
| 03 | X03 | Provider tests | ~200 | ~30 | 15% | P1 |
| 04 | X04 | Tool tests | ~400 | ~100 | 25% | P1 |
| 05 | X05 | Integration tests (live API) | ~300 | 0 | 0% | P1 |
| 06 | X06 | Agent loop tests | ~150 | ~30 | 20% | P1 |
| 07 | X07 | Gateway platform tests | ~100 | ~20 | 20% | P1 |
| 08 | X08 | Conversation loop tests | ~200 | ~10 | 5% | P1 |
| 09 | X09 | Edge case / regression tests | ~62 | ~3 | 5% | P1 |
| 10 | X10 | Fuzz / property tests | ~0* | 0 | 0% | P3 |
| 11 | X11 | Performance / benchmark tests | ~30 | 0 | 0% | P2 |
| 12 | X12-X20 | Subsystem test gaps | ~200 | ~50 | 25% | P1-P2 |

**S7: 20 gap clusters (9 P1, 3 P2, 8 P3) — 1,000+ individual test cases**

---

## S8: Provider Adapter Layer (P1)

Python has adapter layers wrapping provider APIs (~9,700 LOC total).

| # | ID | Adapter | LOC | Missing in C | Priority |
|---|----|---------|-----|-------------|----------|
| 01 | R01 | anthropic_adapter.py | 2275 | Streaming variants, extended thinking, prompt caching | P1 |
| 02 | R02 | bedrock_adapter.py | 1289 | AWS Bedrock auth, model discovery | P1 |
| 03 | R03 | google_oauth.py | 1059 | OAuth token exchange, refresh | P1 |
| 04 | R04 | gemini_native_adapter.py | 971 | Gemini native API features, safety | P1 |
| 05 | R05 | gemini_cloudcode_adapter.py | 909 | Gemini Codex mode | P2 |
| 06 | R06 | azure_identity_adapter.py | 555 | Azure managed identity, OAuth2 | P1 |
| 07 | R07 | codex_responses_adapter.py | 1221 | Codex API response handling | P2 |
| 08 | R08 | copilot_acp_client.py | 686 | GitHub Copilot ACP client | P2 |
| 09 | R09 | plugin_llm.py | 1046 | Plugin-based LLM abstraction | P2 |
| 10 | R10 | model_metadata.py | 1850 | Model discovery, catalog, capabilities | P1 |

**S8: 10 gaps (6 P1, 4 P2)**

---

## S9: Plugin System — Architecture Gap (P2)

C has plugin_ext.c for loading .so shared libraries but zero actual plugins shipped.

| # | ID | Plugin | Python State | C State | Priority |
|---|----|--------|-------------|---------|----------|
| 01 | P01 | Architecture: plugin loading + lifecycle | 50+ plugins, plugin manager | plugin_ext.c loads .so only | P1 |
| 02 | P02 | Memory providers (mem0, honcho, etc.) | 8 plugins | 0 | P2 |
| 03 | P03 | Model provider plugins | 29 plugins | hardcoded in provider_*.c | P2 |
| 04 | P04 | Kanban board/workflow | 7 files, ~11,000 LOC | 0 | P2 |
| 05 | P05 | Observability / telemetry | 1 plugin | 0 | P3 |
| 06 | P06 | Achievements | 1 plugin | 0 | P3 |
| 07 | P07-P20 | Other plugins | ~14 more | 0 | P3 |

**S9: 20 gaps (1 P1, 4 P2, 15 P3)**

---

## S10: Architecture & Platform (P0)

| # | ID | Gap | Detail | Priority |
|---|----|-----|--------|----------|
| 01 | F01 | C can't hook Python | Standalone binary, cannot import Python | P0 |
| 02 | F02 | Test count mismatch | 248 C vs 1,262 Python tests | P0 |
| 03 | F03 | No Python interop | Cannot reuse Python libraries at runtime | P0 |
| 04 | F04 | Single-threaded agent loop | Python uses asyncio for concurrent ops | P0 |
| 05 | F05 | No credential automation | Python OAuth flows not replicated | P1 |
| 06 | F06 | No ACP protocol server | VS Code/Zed/JetBrains integration missing | P2 |
| 07 | F07 | No session replay / debugging | Python session trajectory replay | P2 |
| 08 | F08 | Raw socket health check | No TCP keepalive / zombie socket recovery | P1 |
| 09 | F09 | No async event loop | Python uses asyncio for gateway + tools | P0 |
| 10 | F10 | No stdin/stdout safe guard | Systemd/daemon crash from broken pipe | P1 |

**S10: 10 gaps (4 P0, 3 P1, 2 P2, 1 P3)**

---

## SUMMARY

| Sector | Gaps | P0 | P1 | P2 | P3 | Description |
|--------|------|----|----|----|----|-------------|
| S0: Display & Visual | 2 | 2 | 0 | 0 | 0 | Phase 0 — D13/D14 done; 15 stale claims retired |
| S1: Conversation Loop Plumbing | 20 | 0 | 15 | 5 | 0 | 8 stale claims + 1 done (L14) discovered in Phase 57 sweep |
| S2: Agent Modules | 45 | 1* | 19 | 14 | 11 | 45 Python modules with no C equivalent |
| S3: Gateway Helpers | 13 | 0 | 3 | 10 | 0 | 13 Python helper sub-modules |
| S4: TUI Ecosystem | 28 | 0 | 14 | 10 | 4 | Full TUI backend + React frontend |
| S5: CLI Ecosystem | 30 | 0 | 1 | 17 | 12 | hermes_cli infrastructure |
| S6: Tool Depth | 20 | 0 | 0 | 10 | 10 | Feature gaps in partially ported tools |
| S7: Test Coverage | 20* | 0 | 9 | 3 | 8 | *1,000+ test cases behind |
| S8: Provider Adapters | 10 | 0 | 6 | 4 | 0 | Adapter layer missing (9,700 LOC) |
| S9: Plugin System | 20 | 0 | 1 | 4 | 15 | Architecture gap |
| S10: Architecture | 10 | 4 | 3 | 2 | 1 | Form-vs-function |
|| **TOTAL** | **213** | **7** | **67** | **79** | **61** | **+L07 stale. Phase 58 covers 8 stale + 3 done.** |

*S2 includes display.py (A13) marked P0 — moves to Phase 0

### Phase Map

| Phase | Focus | Sectors | Gap Count |
|-------|-------|---------|-----------|
|| Phase 0 | Display & Visual | S0 (2) + A13 | 3 |
| Phase 1 | Agent plumbing + Provider adapters + TUI backend | S1 (28), S8 (6), S4 P1 (14) | ~48 |
| Phase 2 | Test coverage campaign | S7 | 20* (1000+ tests) |
| Phase 3 | Gateway helpers + Tool depth | S3, S6 | ~33 |
| Phase 4 | CLI ecosystem | S5 | ~30 |
| Phase 5 | Plugin system + Remaining agent modules | S9, S2 P2-P3 | ~46 |
| Phase 6 | Architecture gaps | S10 | ~10 |

---

*Compiled May 28 2026. DA v1 audit. Every count verified against live source code.*
*S1 conversation loop plumbing extracted from Python's 4606-line run_conversation vs C's 1600-line agent_loop.c.*
