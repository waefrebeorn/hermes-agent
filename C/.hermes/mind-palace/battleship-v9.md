# Slermes C — Fresh Battleship (v9 — Full Triple DA Audit)

Generated 2026-05-25 by comprehensive Triple DA: source-code comparison of every C file vs every Python file across all subsystems. Every gap verified against actual source code.

CLI: 79 C vs 69 Python (+10 C). Tools: 85 C vs ~37 Python (+31 C).

**After live testing: discovered 8 critical integration breaks (Sector 0A) + 12 display gaps (Sector 0B). Total: 75 real gaps (~50,000 LOC to port)**

---

## SECTOR 0A: P0 Entry Point Integration (8 gaps)

**CRITICAL — Fix FIRST. All entry points are broken in different ways.** The code compiles and registers correct infrastructure, but runtime behavior diverges from Python Hermes on every entry path. Users cannot even reach the app.

| # | ID | Entry Point | Python Behavior | C Behavior | Root Cause | Priority |
|---|-----|-------------|-----------------|------------|------------|----------|
| 1 | I01 | Pipe mode multi-line | `echo -e "/help\n/status" \| hermes` processes each line as separate command | Reads ALL stdin into one 64KB buffer, dispatches as single command → "Unknown command" for anything multi-line | `cli.c:548-588` reads full stdin via fgetc loop, single dispatch at end. Should split by lines. | **P0** |
| 2 | I02 | Unknown flags | `hermes --bogus` → error message, no LLM call | Sends `--bogus` to agent as user message → consumes API credits + returns irrelevant response | `main.c` arg parser doesn't reject unknown flags, falls through to interactive/pipe mode | **P0** |
| 3 | I03 | TUI flag | `hermes --tui` → launches ncurses TUI | `hermes --tui` → sends "tui" as user message to LLM. Flag parsed as positional arg. | `main.c` arg parser validates `--tui` presence but doesn't branch to TUI mode; falls into agent path | **P0** |
| 4 | I04 | `--session` without value | `hermes --session` → error "expected argument" | `hermes --session` → consumes next argv as session ID despite it being empty/missing. With pipe: triggers LLM call. | C doesn't require --session arg, treats missing as `argv[2]` being the session ID | **P0** |
| 5 | I05 | Log file location | `hermes logs` → reads `~/.hermes/logs/agent.log` | `slermes logs` → reads `~/.hermes/logs/agent.log` (Python Hermes logs!) instead of `~/.slermes/logs/slermes.log` | `logs` command hardcodes Python log path, not slermes log path | **P0** |
| 6 | I06 | Config home resolution | `SLERMES_HOME` → `HERMES_HOME` → `~/.slermes/` config.yml | `~/.slermes/config.yaml` (mixed YAML/yml extension). Plugin dir. | Config path extension mismatch (yaml vs yml), inconsistent fallback chain | **P0** |
| 7 | I07 | Provider model mismatch | `deepseek/deepseek-v4-flash` works with proper thinking/reasoning params | DeepSeek API error: "reasoning_content must be passed back" — provider sends wrong params | Provider config in `provider_deepseek.c` sends `reasoning_content` param that DeepSeek requires | **P0** |
| 8 | I08 | Cron hangs | `slermes cron` → shows scheduler status | `slermes cron` → hangs forever (timeout) | Cron init with no jobs/schedule enters infinite wait, no zero-state display | **P0** |

---

## SECTOR 0B: P0 Display & Visual Parity (12 gaps)

**Critical prerequisite for user-facing quality.** Every CLI visual element must match Python Hermes 1:1.

Current: 8 ANSI colors, basic `|/-\` spinner, simple `┌─┐` panel, bare printf banners. Python: Rich panels, KawaiiSpinner, skin engine (30+ hex colors), status bar, tool prefix, prompt_toolkit.

| # | ID | Area | Python Source | C State | LOC | Priority |
|---|-----|------|-------------|---------|-----|----------|
| 1 | V01 | Skin Engine | `hermes_cli/skin_engine.py` (926 LOC) | None — 8 hardcoded ANSI colors in `deps/cli_display.c` | 926 | **P0** |
| 2 | V02 | KawaiiSpinner | `agent/display.py` (~600 LOC) | Basic `|/-\` spinner, no faces, no verbs, 9 types missing | 600 | **P0** |
| 3 | V03 | Rich Banner | `cli.py` Rich panels | `print_banner()`: no ASCII art gradient, no panels | 400 | **P0** |
| 4 | V04 | Status Bar | `cli.py` | None | 300 | **P0** |
| 5 | V05 | Tool Activity Feed | `cli.py` ┊ prefix + emoji | Raw printf; prefix exists in tool_event_cb but no emoji | 200 | **P0** |
| 6 | V06 | Response Box | `cli.py` panel | `cli_display_response()` plain ANSI, no box | 200 | **P0** |
| 7 | V07 | Rich Help / Commands | `cli.py` Rich table | `cmd_help()` raw text list | 300 | **P0** |
| 8 | V08 | ANSI 256/TrueColor | Rich lib | 8 hardcoded ANSI (30-37). `display_set_fg_rgb` exists in cli.c but not wired | 300 | **P0** |
| 9 | V09 | Prompt Input | prompt_toolkit | `fgets()` via line_edit — no tab complete, history, multiline | 800 | **P0** |
| 10 | V10 | Markdown Rendering | Rich markdown | `markdown_tables.c` for parsing only, no rendering | 400 | **P0** |
| 11 | V11 | Kawaii Faces/Verbs | 15 faces, 15 verbs, 9 styles | None | 0 (data) | **P0** |
| 12 | V12 | Tool Emoji Registry | skin engine tool_emojis | None | 200 | **P0** |

---

## SECTOR 1: P1 Agent Modules (4 gaps)

| # | ID | Source | Issue | LOC |
|---|----|--------|-------|-----|
| 1 | E01 | agent/error_classifier.py | Structured API error taxonomy | 1,087 |
| 2 | E02 | agent/chat_completion_helpers.py | Message formatting, token counting, param normalization | 2,078 |
| 3 | E03 | agent/tool_executor.py | Structured tool dispatch with validation/timeout | 910 |
| 4 | E04 | tools/process_registry.py | Managed background process lifecycle | 1,544 |

---

## SECTOR 2: P2 Missing Python Tools (14 gaps)

| # | ID | Source | LOC |
|---|----|--------|-----|
| 1 | T01 | browser_camofox.py | 746 |
| 2 | T02 | mcp_oauth.py + manager.py | 1,256 |
| 3 | T03 | yuanbao_tools.py | 736 |
| 4 | T04 | microsoft_graph_auth.py + client.py | 799 |
| 5 | T05 | credential_files.py | 436 |
| 6 | T06 | website_policy.py | 282 |
| 7 | T07 | osv_check.py | 155 |
| 8 | T08 | clarify_gateway.py | 278 |
| 9 | T09 | interrupt.py | 98 |
| 10 | T10 | env_passthrough.py | 152 |
| 11 | T11 | budget_config.py | 189 |
| 12 | T12 | schema_sanitizer.py | 234 |
| 13 | T13 | fuzzy_match.py | 703 |
| 14 | T14 | slash_confirm.py | 167 |

---

## SECTOR 3: P2 Missing Provider Adapters (7 gaps)

| # | ID | Source | LOC |
|---|----|--------|-----|
| 1 | A01 | anthropic_adapter.py | 2,220 |
| 2 | A02 | bedrock_adapter.py | 1,289 |
| 3 | A03 | azure_identity_adapter.py | 555 |
| 4 | A04 | gemini_cloudcode_adapter.py | 909 |
| 5 | A05 | gemini_native_adapter.py | 971 |
| 6 | A06 | google_oauth.py + code_assist.py | 1,513 |
| 7 | A07 | codex_responses_adapter.py + codex_runtime.py | 1,532 |

---

## SECTOR 4: P2 Missing Gateway Sub-Modules (6 gaps)

| # | ID | Source | LOC |
|---|----|--------|-----|
| 1 | G01 | feishu_comment.py + rules.py | 457 |
| 2 | G02 | wecom_callback.py + crypto.py | 445 |
| 3 | G03 | yuanbao_media.py + proto.py + sticker.py | 699 |
| 4 | G04 | signal_rate_limit.py | 78 |
| 5 | G05 | telegram_network.py | 234 |
| 6 | G06 | helpers.py + base.py + _http_client_limits.py | 889 |

---

## SECTOR 5: P2 Missing Agent Modules (6 gaps)

| # | ID | Source | LOC | Notes |
|---|----|--------|-----|-------|
| 1 | M01 | agent/insights.py | 930 | /insights placeholder |
| 2 | M02 | memory_manager.py + memory_provider.py | 888 | C memory.c hardcodes SQLite |
| 3 | M03 | rate_limit_tracker.py | 246 | Provider header parsing |
| 4 | M04 | models_dev.py | 723 | Remote model discovery |
| 5 | M05 | curator_backup.py | 693 | Skill backup/rollback |
| 6 | M06 | context_compressor.py | 1,748 | Adaptive compression |

---

## SECTOR 6: P3 Missing Modules (7 gaps)

| # | ID | Source | LOC |
|---|----|--------|-----|
| 1 | N01 | credential_sources.py | 448 |
| 2 | N02 | plugin_llm.py | 1,046 |
| 3 | N03 | model_metadata.py | 1,827 |
| 4 | N04 | prompt_builder.py | 1,465 |
| 5 | N05 | skill_utils.py | 511 |
| 6 | N06 | background_review.py | 582 |
| 7 | N07 | stream_diag.py | 280 |

---

## SECTOR 7: P3 Tool Depth Gaps (7 gaps)

| # | ID | Tool | C LOC | Python LOC | Ratio | Missing |
|---|-----|------|-------|------------|-------|---------|
| 1 | D01 | browser.c | 1,598 | 3,796 | 42% | Autofill, cookies, PDF, HAR |
| 2 | D02 | vision.c | 203 | 1,421 | 14% | OCR, face detect, barcode, EXIF |
| 3 | D03 | web.c | 466 | 1,561 | 30% | Cookie jar, sessions, proxy, forms |
| 4 | D04 | mcp_tool.c | 1,623 | 3,584 | 45% | SSE, OAuth, subscriptions, sampling |
| 5 | D05 | file.c | 561 | 1,220 | 46% | Glob, fswatch, diff, hex view |
| 6 | D06 | feishu_tools.c | 210 | 872 | 24% | Sheets, chat, calendar, approvals |
| 7 | D07 | computer_use | 0 | 824 | 0% | Not ported |

---

## SECTOR 8: P3 Plugin System (4 gaps)

| # | ID | Area | Effort |
|---|-----|------|--------|
| 1 | P01 | 29 model-provider plugins | Architecture + wiring |
| 2 | P02 | 8 memory provider plugins | Abstraction layer |
| 3 | P03 | Other plugins (10) | Wiring |
| 4 | P04 | Plugin LLM (1,046 LOC) | Module port |

---

## Summary

| Sector | Category | Gaps | Total LOC | Priority |
|--------|----------|------|-----------|----------|
| 0A | Entry Point Integration | 8 | ~1,000 (fixes) | **P0** |
| 0B | Display & Visual Parity | 12 | ~4,200 | **P0** |
| 1 | P1 Agent Modules | 4 | ~5,600 | 🔴 |
| 2 | P2 Missing Tools | 14 | ~5,600 | 🟡 |
| 3 | P2 Missing Providers | 7 | ~9,700 | 🟡 |
| 4 | P2 Gateway Sub-modules | 6 | ~2,800 | 🟡 |
| 5 | P2 Missing Agent Modules | 6 | ~5,200 | 🟡 |
| 6 | P3 Missing Modules | 7 | ~6,200 | 🟢 |
| 7 | P3 Tool Depth | 7 | ~0 (improvements) | 🟢 |
| 8 | P3 Plugin System | 4 | ~0 (architecture) | 🟢 |
| **Total** | | **75** | **~50,000** | |

## Phase Order (Execution Roadmap)
1. **Phase 0a — Entry Points** (8 gaps): Fix pipe mode multi-line dispatch → fix unknown flags → fix TUI flag → fix --session arg → fix log path → fix config home → fix provider model → fix cron zero-state
2. **Phase 0b — Display** (12 gaps): Skin engine → Spinner → Banner → Status bar → Tool prefix → Response box → Help → 256-color → Prompt input → Markdown → Faces → Emoji registry
3. **Phase 1 — P1 Agent Modules** (4 gaps): error_classifier → chat_completion_helpers → tool_executor → process_registry
4. **Phase 2 — P2 Ports** (33 gaps): Missing tools → Provider adapters → Gateway modules → Agent modules
5. **Phase 3 — P3 Depth** (18 gaps): Smaller modules → Tool depth → Plugin system
