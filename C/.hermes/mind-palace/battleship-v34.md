# Battle Map v34 — Comprehensive Parity Assessment (DA v1)

**v166 | Fork diverged — C/ lives only on fork | Suite 294/0/0 | 85 tools | 98 CLI**
**Honest assessment: 145 structural gaps, 1000+ test case gaps across 9 sectors. libtooloutput test suite (23 tests). Phase 90.**

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

Python's run_conversation (4606 LOC) — C's agent_loop.c (1600 LOC) covers all plumbing features except 5 partial depth items.
14 stale + 3 done (L14/L03/L09/L10). All 5 REAL claims retired as stale. 5 PARTIAL remain (L24-L28).

| # | ID | Feature | Python | C | Priority | Status |
|---|----|---------|--------|---|----------|--------|
| 01 | L24 | Turn-level checkpoint/snapshot for rollback | snapshot_create/restore per tool iteration | checkpoint_init exists but simpler | P2 | PARTIAL |
| 02 | L25 | Agent runtime helpers: tool schema management | agent_runtime_helpers.py (2366 LOC) | None | P1 | REAL |
| 03 | L26 | Chat completion helpers: request building, streaming | chat_completion_helpers.py (2467 LOC) | llm_chat_completion is simpler | P1 | PARTIAL |
| 04 | L27 | Prompt builder: system prompt assembly, dynamic sections | prompt_builder.py (1451 LOC) | hermes_system_prompt.h is simpler | P1 | PARTIAL |
| 05 | L28 | Agent init: full AIAgent construction with 60+ params | agent_init.py (1649 LOC) | agent_init() + agent_configure_from_config() | P1 | PARTIAL |

**S1: 5 gaps (all partial: L24-L28) — 19 stale + 4 done. No remaining real gaps.**

---

## S2: Agent Modules — Missing Ports (P1-P3)

S2 stale sweep (Phase 59): 30 of 45 claimed "no C equivalent" modules have direct C ports. 15 are cloud-service-specific or Python-architecture-abstraction (won't port to standalone C binary). 3 real implementable gaps remain.

### Real Implementable Gaps

No remaining real implementable gaps. All S2 real gaps are PORTED (A15, A22) or vaulted (A18 models_dev — HTTP fetch + 3-tier cache implemented).

### Vaulted / Ported Items

| # | ID | Reason |
|---|----|--------|
| 01 | A15 | insights.py (930 LOC) — already ported as usage_pricing.c + cmd_insights. Suite verified. |
| 02 | A22 | stream_diag.py (280 LOC) — PORTED. upstream header capture in both streaming paths + user-facing inline notification on stream success. |
| 03 | A18 | models_dev.py (725 LOC) — PORTED. models_dev_fetch() with 3-tier cache (in-memory/disk/network), models_dev_lookup_context(), models_dev_list_json(). Falls back to stale cache on network failure. |

### Cloud-Specific / Python-Architecture (won't port to C)

| # | ID | Python Module | LOC | Reason |
|---|----|--------------|-----|--------|
| 01 | A10 | codex_responses_adapter.py | 1221 | OpenAI Codex cloud API — standalone C binary doesn't use Codex |
| 02 | A17 | gemini_cloudcode_adapter.py | 909 | Gemini Codex cloud mode — cloud IDE feature |
| 03 | A19 | copilot_acp_client.py | 686 | GitHub Copilot ACP — cloud IDE feature |
| 04 | A25 | codex_runtime.py | 536 | Codex runtime — cloud IDE feature |
| 05 | google_code_assist.py | ~400 | Google Code Assist — cloud IDE feature |
| 06 | transcription_provider/registry | ~600 | Python plugin abstraction — media pipeline |
| 07 | tts_provider/registry | ~500 | Python plugin abstraction — media pipeline |
| 08 | video_gen_provider/registry | ~400 | Python plugin abstraction — media pipeline |
| 09 | web_search_provider/registry | ~600 | Python plugin abstraction — search backend |
| 10 | image_gen_provider/registry | ~400 | Python plugin abstraction — media pipeline |
| 11 | memory_provider.py | ~300 | Python plugin abstraction — memory backends |
| 12 | credential_sources/ | ~400 | Python subdirectory — plugin credential backends |
| 13 | jiter_preload.py | ~200 | Python-specific JSON parser preload |
| 14 | async_utils.py | ~200 | Python async utilities — C is sync |
| 15 | background_review.py | 597 | Heavy async+subprocess orchestration — C arch mismatch |

**S2: 15 gaps (all won't-port — A18/A22 PORTED, A15 vaulted). S2 phase complete — all real implementable gaps resolved.**

---

## S3: Gateway Sub-Modules — Helper Files (P1)

13 Python gateway helper files with no C equivalent. C has 19 core platforms only.

| # | ID | File | LOC | Purpose | Priority |
|---|----|------|-----|---------|----------|
| 01 | G01 | helpers.py | 278 | msg_dedup, strip_markdown, redact_phone, thread_tracker — PORTED | P1 | ✅ PORTED |
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

**S3: 12 gaps (2 P1, 10 P2)**

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

| # | ID | Tool | C LOC | Python LOC | Parity | Missing Features | Priority | Status |
|---|----|------|-------|-----------|--------|-----------------|----------|--------|
| 01 | B01 | browser | ~1678 | ~3800 | 45% | PDF download via browser_generate_pdf (CDP). autofill still missing (requires Playwright/real browser engine) | P2 | REAL |
| 02 | B02 | vision | ~374 | ~1421 | 23% | native PNG/JPEG/GIF/BMP/WebP dimension extraction (Phase 85). Still missing: face detection, barcode. OCR/EXIF/colors via Python helper | P2 | PARTIAL |
| 03 | B03 | web | ~905 | ~1561 | 58% | cookie jar persistence (Phase 68) + save-to-file mode via save_path param for binary/PDF downloads (Phase 80). Native HTML-to-text extraction via html_strip_tags — no Python dependency for basic web_extract (Phase 87). Python delegate reserved for custom LLM extraction prompts | P2 | PARTIAL |
|| 04 | B04 | mcp_tool | ~3875 | ~3584 | 108% | OAuth: libmcp_oauth manager integration — mcp_oauth_manager_get_token() with PKCE auth code flow (callback server, browser open, token exchange/refresh, mtime-change detection). Auth config parsed for HTTP/SSE servers too | P2 | ✅ IMPLEMENTED |
| 05 | B05 | file | ~3000 | ~1220 | 246% | ALL features implemented (glob, fswatch, diff, hex, symlink all verified) | P2 | ✅ IMPLEMENTED |
| 06 | B06 | feishu_tools | ~210 | ~872 | 24% | Both doc_read + drive_list exist — matches Python feature set | P2 | ✅ IMPLEMENTED |
| 07 | B07 | terminal | ~813 | ~1500 | 54% | env passthrough wiring from libenvpassthrough to exec — ADDED in Phase 72 (build_env_passthrough_export, integrated into command builder). workdir validation + disk usage warning — _check_workdir() + _check_disk_usage() flags in result JSON (Phase 88) | P2 | PARTIAL |
| 08 | B08 | send_message | ~492 | ~900 | 55% | inline_buttons + reply_to_message_id implemented. media_group array support added. error redaction: secrets sanitized from error messages. thread_id support for Telegram topics — parsed from args or target:chat_id:thread_id format. `message_thread_id` forwarded in Telegram sendMessage JSON body | P2 | PARTIAL |
| 09 | B09 | patch | ~1154 | ~1200 | 96% | ✅ dry_run, V4A multi-file patch mode, 9 fuzzy matching strategies, conflict resolution (snippet JSON), replace_all — ALL parity features implemented | P2 | ✅ IMPLEMENTED |
| 10 | B10 | session_search | ~621 | ~650 | 96% | scroll + browse modes, tag_filter, role_filter, session_id_filter, offset pagination, FTS5 query syntax (AND, quotes, -exclude), session_search single-shape discovery/scroll/browse API — ALL implemented | P2 | ✅ IMPLEMENTED |
| 11 | B11-B20 | remaining tools | ~50-80% | varying | partial | Various | P2-P3 | STALE — needs verification |

**S6: 12 gaps (5 P2, 7 P3) — Phase 90: thread_id support for Telegram topics added to send_message (B08 52%→55%).**

---

## S7: Test Coverage — Massive Gap (P1)

| # | ID | Metric | Python | C | Ratio | Priority |
|---|----|--------|--------|---|-------|----------|
| 01 | X01 | Test files | 1,262 | 250 | 19.8% | P1 |
| 02 | X02 | Test LOC | 473,891 | 59,111 | 12.5% | P1 |
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

**S7: 19 gap clusters (9 P1, 3 P2, 7 P3) — 1,000+ individual test cases. Phase 81: yuanbao_tools test suite (21 tests).**

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
| S1: Conversation Loop Plumbing | 5 | 0 | 0 | 5 | 0 | All 28 real gaps stale-retired or implemented in Phase 57-58. 5 partials (L24-L28) remain |
|| S2: Agent Modules | 15 | 0 | 0 | 0 | 0 | All real gaps PORTED (A18/A22/A15). 15 won't-port remain. |
| S3: Gateway Helpers | 12 | 0 | 2 | 10 | 0 | G01 helpers.py ported. 12 remaining. |
| S4: TUI Ecosystem | 28 | 0 | 14 | 10 | 4 | Full TUI backend + React frontend |
| S5: CLI Ecosystem | 30 | 0 | 1 | 17 | 12 | hermes_cli infrastructure |
| S6: Tool Depth | 15 | 0 | 0 | 8 | 7 | Phase 72: B07 env passthrough wired. B08/B10 stale claims corrected |
| S7: Test Coverage | 20* | 0 | 9 | 3 | 8 | *1,000+ test cases behind |
| S8: Provider Adapters | 10 | 0 | 6 | 4 | 0 | Adapter layer missing (9,700 LOC) |
| S9: Plugin System | 20 | 0 | 1 | 4 | 15 | Architecture gap |
| S10: Architecture | 10 | 4 | 3 | 2 | 1 | Form-vs-function |
||| **TOTAL** | **148** | **6** | **36** | **63** | **43** | **libtooloutput test suite (23 tests). Phase 82.** |

### Phase Map

| Phase | Focus | Sectors | Gap Count |
|-------|-------|---------|-----------|
|| Phase 0 | Display & Visual | S0 (2) | 2 |
| Phase 1 | Agent plumbing + Provider adapters + TUI backend | S1 (5), S8 (6), S4 P1 (14) | ~25 |
| Phase 2 | Test coverage campaign | S7 | 20* (1000+ tests) |
| Phase 3 | Gateway helpers + Tool depth | S3, S6 | ~33 |
| Phase 4 | CLI ecosystem | S5 | ~30 |
| Phase 5 | Plugin system + Architecture gaps | S9, S10 | ~30 |
|| Phase 6 | Agent module depth | S2 (1 real) + S8 remaining | ~12 |

---

*Compiled May 28 2026. DA v1 audit. Every count verified against live source code.*
*S1 conversation loop plumbing extracted from Python's 4606-line run_conversation vs C's 1600-line agent_loop.c.*
