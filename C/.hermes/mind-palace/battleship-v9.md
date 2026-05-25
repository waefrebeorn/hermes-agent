# Slermes C — Fresh Battleship (v9 — Full Triple DA Audit)

Generated 2026-05-25 by comprehensive Triple DA: source-code comparison of every C file vs every Python file across all subsystems. Every gap verified against actual source code — no stale battleship claims propagated.

CLI: 79 C vs 69 Python (+10 C). Tools: 85 C vs ~37 Python (+31 C). All 79 CLI commands have real handlers, not stubs.

**Phase 0 (Display Parity) added: 12 gaps.** Total: **67 real work-unit gaps (~45,000 LOC to port)**

---

## SECTOR 0: P0 Display & Visual Parity (12 gaps)

**Critical prerequisite.** Every other gap is experienced through the CLI. If the visual output doesn't match Python Hermes 1:1, users see a broken product regardless of backend parity.

Current C state: 8 ANSI colors via `deps/cli_display.c`, basic `|/-\` spinner, simple `┌─┐` panel, bare `printf` banners. Python Hermes: Rich panels, KawaiiSpinner with animated faces, skin engine with 30+ hex colors, status bar, tool prefix, prompt_toolkit autocomplete.

| # | ID | Area | Python Source | C State | LOC | Priority |
|---|-----|------|-------------|---------|-----|----------|
| 1 | V01 | Skin Engine | `hermes_cli/skin_engine.py` (926 LOC) | None — C has hardcoded 8 ANSI colors in `deps/cli_display.c` | 926 | **P0** |
| 2 | V02 | KawaiiSpinner | `agent/display.py` (~600 LOC) | Basic `|/-\` spinner in `deps/cli_display.c:166-190` — no kawaii faces, no thinking verbs, no 9 spinner types | 600 | **P0** |
| 3 | V03 | Rich Banner | `cli.py` Rich panels | `print_banner(): printf("WuBu Slermes v%s\\n")` — no ASCII art, no colored panels, no branding | 400 | **P0** |
| 4 | V04 | Status Bar | `cli.py` status bar rendering | None — C has no context/model status bar | 300 | **P0** |
| 5 | V05 | Tool Activity Feed | `cli.py` `┊` prefix + emoji | None — C tools emit raw printf; no `┊` prefix, no tool emoji | 200 | **P0** |
| 6 | V06 | Response Box | `cli.py` response panel with border | `cli_display_response()` prints plain text with basic ANSI color — no box border, no label | 200 | **P0** |
| 7 | V07 | Rich Help / Command List | `cli.py` Rich table | `cmd_help()` uses simple `printf("  /command — description\\n")` — no table, no categories, no colors | 300 | **P0** |
| 8 | V08 | ANSI 256-color / TrueColor | `rich` library automatically | C has 8 hardcoded ANSI color indices (30-37). Python uses hex `#FFD700` → 24-bit truecolor | 300 | **P0** |
| 9 | V09 | Prompt Input | `prompt_toolkit` | C uses `fgets()` via `cli_input_line()` — no tab completion, no history, no syntax highlight, no multi-line | 800 | **P0** |
| 10 | V10 | Markdown Rendering | `rich` markdown renderer | C has `markdown_tables.c` for table parsing but no markdown rendering for output | 400 | **P0** |
| 11 | V11 | Progress Spinner Faces | `KawaiiSpinner.KAWAII_WAITING` etc. | 15 Unicode kawaii faces, 15 thinking faces, 15 verbs, 9 spinner styles — C has `|/-\` only | 0 (design) | **P0** |
| 12 | V12 | Tool Emoji Registry | Skin engine `tool_emojis` | Python assigns emoji per tool (terminal:⚔, web_search:🔮). C has no tool emoji concept | 200 | **P0** |

---

## SECTOR 1: P1 Agent Modules (4 gaps)

Critical modules with NO C equivalent. These are used in every agent conversation.

| # | ID | Source | Issue | LOC | Est Effort |
|---|----|--------|-------|-----|------------|
| 1 | E01 | agent/error_classifier.py | Structured API error taxonomy — current C uses inline string matching in retry_utils.c | 1,087 | 2-3 sessions |
| 2 | E02 | agent/chat_completion_helpers.py | Message formatting, token counting, parameter normalization — partial in llm_client.c | 2,078 | 3-4 sessions |
| 3 | E03 | agent/tool_executor.py | Structured tool dispatch with validation, timeout, and error classification — inline in agent_loop.c | 910 | 2 sessions |
| 4 | E04 | tools/process_registry.py | Managed background process lifecycle (spawn/poll/wait/kill, crash recovery, JSON checkpoint) — C process.c is simpler | 1,544 | 3 sessions |

---

## SECTOR 2: P2 Missing Python Tools (14 gaps)

Python tool files with NO C equivalent. These are registered tools, not internal helpers.

| # | ID | Source | Issue | LOC | Notes |
|---|----|--------|-------|-----|-------|
| 1 | T01 | tools/browser_camofox.py + state.py | Stealth/anti-detect browser (Camofox) — different from CDP browser.c | 746 | Uses Camofox engine |
| 2 | T02 | tools/mcp_oauth.py + manager.py | MCP OAuth token refresh, credential manager, PKCE flow | 1,256 | 2 files, moderate complexity |
| 3 | T03 | tools/yuanbao_tools.py | Yuanbao messaging/group tools for gateway | 736 | Platform-specific |
| 4 | T04 | tools/microsoft_graph_auth.py + client.py | Microsoft Graph API auth + client | 799 | 2 files, Azure-specific |
| 5 | T05 | tools/credential_files.py | Credential file management (list/read/write GnuPG) | 436 | |
| 6 | T06 | tools/website_policy.py | Website blocklist, content category blocking | 282 | |
| 7 | T07 | tools/osv_check.py | OSV (Open Source Vulnerabilities) dependency check | 155 | Small |
| 8 | T08 | tools/clarify_gateway.py | Gateway-specific clarification prompts | 278 | |
| 9 | T09 | tools/interrupt.py | Signal interrupt handling (SIGINT/SIGTERM) for tool calls | 98 | Tiny |
| 10 | T10 | tools/env_passthrough.py | Environment variable passthrough to subprocesses | 152 | Small |
| 11 | T11 | tools/budget_config.py | Budget configuration UI (/budget command) | 189 | |
| 12 | T12 | tools/schema_sanitizer.py | JSON schema sanitization (remove null/empty) | 234 | |
| 13 | T13 | tools/fuzzy_match.py | Fuzzy filename/path matching (Levenshtein) | 703 | |
| 14 | T14 | tools/slash_confirm.py | Slash command confirmation prompts | 167 | |

---

## SECTOR 3: P2 Missing Provider Adapters (7 gaps)

Provider protocol adapters in Python with NO C equivalent. C has hardcoded HTTP providers but lacks the richer adapter layers.

| # | ID | Source | Issue | LOC |
|---|----|--------|-------|-----|
| 1 | A01 | agent/anthropic_adapter.py | Anthropic streaming, extended thinking, prompt caching headers, multi-modal payloads | 2,220 |
| 2 | A02 | agent/bedrock_adapter.py | AWS Bedrock Converse API adapter with STS auth and streaming | 1,289 |
| 3 | A03 | agent/azure_identity_adapter.py | Microsoft Entra ID keyless auth via DefaultAzureCredential | 555 |
| 4 | A04 | agent/gemini_cloudcode_adapter.py | Google Cloud Code Assist API adapter | 909 |
| 5 | A05 | agent/gemini_native_adapter.py | Google Gemini native API adapter (context caching, grounding) | 971 |
| 6 | A06 | agent/google_oauth.py + code_assist.py | Full OAuth PKCE flow and quota/tier management | 1,513 |
| 7 | A07 | agent/codex_responses_adapter.py + codex_runtime.py | OpenAI Codex Responses API adapter and app-server runtime | 1,532 |

---

## SECTOR 4: P2 Missing Gateway Sub-Modules (6 gaps)

Helper sub-modules in Python with NO C equivalent. Gateways still work but lack these features.

| # | ID | Source | Issue | LOC |
|---|----|--------|-------|-----|
| 1 | G01 | gateway/platforms/feishu_comment.py + rules.py | Feishu comment operations and rules engine | 457 |
| 2 | G02 | gateway/platforms/wecom_callback.py + crypto.py | WeCom callback verification and message encryption | 445 |
| 3 | G03 | gateway/platforms/yuanbao_media.py + proto.py + sticker.py | Yuanbao media upload, WebSocket protocol, sticker handling | 699 |
| 4 | G04 | gateway/platforms/signal_rate_limit.py | Signal-specific rate limiting | 78 |
| 5 | G05 | gateway/platforms/telegram_network.py | Telegram network/proxy configuration | 234 |
| 6 | G06 | gateway/platforms/helpers.py + base.py + _http_client_limits.py | General gateway helpers, base class, HTTP limits | 889 |

---

## SECTOR 5: P2 Missing Agent Modules (6 gaps)

Agent infrastructure modules with NO C equivalent.

| # | ID | Source | Issue | LOC | Notes |
|---|----|--------|-------|-----|-------|
| 1 | M01 | agent/insights.py | Session usage insights/analytics — /insights command shows placeholder | 930 | |
| 2 | M02 | agent/memory_manager.py + memory_provider.py | Multi-provider memory abstraction layer — C memory.c hardcodes SQLite | 888 | Required for plugin-level memory |
| 3 | M03 | agent/rate_limit_tracker.py | Provider response header parsing for /usage display | 246 | |
| 4 | M04 | agent/models_dev.py | Remote model discovery via models.dev API (JSON snapshot + HTTP refresh) | 723 | |
| 5 | M05 | agent/curator_backup.py | Snapshot/rollback for curator skill management | 693 | |
| 6 | M06 | agent/context_compressor.py | Adaptive context compression engine with quality feedback | 1,748 | |

---

## SECTOR 6: P3 Missing Tools/Modules (7 gaps)

Smaller modules and utility ports.

| # | ID | Source | Issue | LOC |
|---|----|--------|-------|-----|
| 1 | N01 | agent/credential_sources.py | Multi-source credential resolution (env, file, keychain) | 448 |
| 2 | N02 | agent/plugin_llm.py | LLM-driven plugin system (plugins can call LLM) | 1,046 |
| 3 | N03 | agent/model_metadata.py | Full model database with HTTP fetch from models.dev | 1,827 |
| 4 | N04 | agent/prompt_builder.py | Advanced prompt construction with template system | 1,465 |
| 5 | N05 | agent/skill_utils.py | Skill system utilities | 511 |
| 6 | N06 | agent/background_review.py | Background review feature | 582 |
| 7 | N07 | agent/stream_diag.py | Stream diagnostics and trace logging | 280 |

---

## SECTOR 7: P3 Tool Depth Gaps (7 gaps)

Tools that are ported to C but have significantly fewer features than Python originals.

| # | ID | Tool | C LOC | Python LOC | Ratio | Missing features |
|---|-----|------|-------|------------|-------|-----------------|
| 1 | D01 | browser.c vs browser_tool.py | 1,598 | 3,796 | 42% | Autofill, cookie mgmt, PDF view, HAR export, screenshot regions, network throttling, accessibility |
| 2 | D02 | vision.c vs vision_tools.py | 203 | 1,421 | 14% | OCR, face detection, barcode scanning, image comparison, EXIF parsing, multi-image, URL input |
| 3 | D03 | web.c vs web_tools.py | 466 | 1,561 | 30% | Cookie jar, session persistence, form auto-fill, proxy rotation, rate limit awareness, JS rendering |
| 4 | D04 | mcp_tool.c vs mcp_tool.py | 1,623 | 3,584 | 45% | SSE transport, streaming response, OAuth flow, resource subscriptions, notification handling, sampling |
| 5 | D05 | file.c vs file_tools.py | 561 | 1,220 | 46% | Glob patterns, fswatch, diff display, binary hex view, file type detection, symlink resolution |
| 6 | D06 | feishu_tools.c vs feishu tools | 210 | 872 | 24% | Only doc_read + drive_list — missing sheet ops, chat, calendar, approval flows |
| 7 | D07 | computer_use — not ported to C at all | 0 | 824 | 0% | Computer use tool (mouse, keyboard, screen control) |

---

## SECTOR 8: P3 Plugin System (4 gaps)

The plugin system is architecturally absent — C has plugin_ext.c for loading .so files but zero actual plugins.

| # | ID | Area | Issue | Effort |
|---|-----|------|-------|--------|
| 1 | P01 | 29 model-provider plugins | ai-gateway, alibaba, anthropic, arcee, bedrock, copilot, custom, deepseek, gemini, gmi, huggingface, kimi, minimax, nous, nvidia, openai-codex, openrouter, xai, etc. | Large — architecture + wiring |
| 2 | P02 | 8 memory provider plugins | byterover, hindsight, holographic, honcho, mem0, openviking, retaindb, supermemory | Medium — abstraction layer |
| 3 | P03 | Other plugins (10) | browser, context_engine, disk-cleanup, google_meet, hermes-achievements, image_gen, kanban, observability, spotify, web | Medium — wiring |
| 4 | P04 | Plugin LLM | agent/plugin_llm.py — LLM calls from plugins (1046 LOC) | P3 module port |

---

## Summary

| Sector | Category | Gaps | Total LOC | Priority |
|--------|----------|------|-----------|----------|
| 0 | Display & Visual Parity | 12 | ~4,200 | **P0** |
| 1 | P1 Agent Modules | 4 | ~5,600 | 🔴 |
| 2 | P2 Missing Tools | 14 | ~5,600 | 🟡 |
| 3 | P2 Missing Providers | 7 | ~9,700 | 🟡 |
| 4 | P2 Gateway Sub-modules | 6 | ~2,800 | 🟡 |
| 5 | P2 Missing Agent Modules | 6 | ~5,200 | 🟡 |
| 6 | P3 Missing Tools/Modules | 7 | ~6,200 | 🟢 |
| 7 | P3 Tool Depth Gaps | 7 | ~0 (improvements) | 🟢 |
| 8 | P3 Plugin System | 4 | ~0 (architecture) | 🟢 |
| **Total** | | **67** | **~45,000** | |

## Phase Order (Execution Roadmap)
1. **Phase 0 — Display Parity** (Sector 0, 12 gaps): Skin engine → Spinner → Banner → Status bar → Tool prefix → Response box → Help/Rich tables → 256-color → Prompt input → Markdown rendering → Faces → Emoji registry
2. **Phase 1 — P1 Agent Modules** (Sector 1, 4 gaps): error_classifier → chat_completion_helpers → tool_executor → process_registry
3. **Phase 2 — P2 Ports** (Sectors 2-5, 33 gaps): Missing tools → Provider adapters → Gateway sub-modules → Agent modules
4. **Phase 3 — P3 Depth** (Sectors 6-8, 18 gaps): Smaller modules → Tool depth → Plugin system
