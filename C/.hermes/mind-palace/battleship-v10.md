# Slermes C — Fresh Battleship v10 (Triple DA — 357 Gaps)

Generated 2026-05-25 by exhaustive Triple DA: stub hunt (20+ patterns), Python-vs-C function-level comparison (75+ tool .py files vs 46 .c files), gateway depth audit, provider feature audit, dead code scan, upstream sync, live binary testing, and command behavioral audit.

Total: **354 active gaps** across 20 sectors.

---

## SECTOR 0A: Entry Point Integration ✅ (8/8 — Phase 0a complete)

All 8 entry point gaps resolved in Phase 0a:
- I01: Dynamic stdin buffer (no fixed 64KB cap)
- I02: Unknown flags → error (not sent to LLM)
- I03: --tui without ncurses → error
- I04: --session requires value
- I05: /logs uses hermes_log_dir() path resolution
- I06: Fallback to config.yml if config.yaml missing
- I07: DeepSeek V4 sends explicit thinking.type=enabled
- I08: Cron exits early with message if no jobs

## SECTOR 0B: Display & Visual Parity (9 gaps)

| # | ID | Feature | Python Source | C State |
|---|-----|---------|-------------|---------|
| 9 | V04 | Status bar (context %, model, session) | cli.py | None |
| 10 | V05 | Tool activity feed (┊ prefix + emoji) | cli.py | Raw printf in event_cb |
| 11 | V06 | Response box (colored border, label) | cli.py | Plain ANSI color |
| 12 | V07 | Rich help (table formatting, categories) | cli.py rich tables | Raw text list |
| 13 | V08 | ANSI 256/TrueColor (hex → 24-bit) | rich lib | 8 colors (30-37) |
| 14 | V09 | Prompt input (tab complete, history, multiline) | prompt_toolkit | fgets() via line_edit |
| 15 | V10 | Markdown rendering for LLM responses | rich markdown | Basic table parsing only |
| 16 | V11 | Kawaii faces (15 waiting, 15 thinking, 15 verbs) | display.py | None |
| 17 | V12 | Tool emoji registry (per-tool emoji) | skin_engine | None |

## SECTOR 0C: CLI Behavioral Parity — Commands That Ignore Args (40 gaps)

**40 of 79 C CLI commands accept user input but silently discard it via `(void)args`.** In Python, these commands parse and respect user arguments. This is the single largest usability gap — a user typing `/save my_session` or `/retry 2` gets no error and no behavior change.

| # | ID | Command | Python Behavior | C Behavior |
|---|-----|---------|----------------|------------|
| 21 | A01 | /save [name] | Saves with optional name | Ignores name, always saves as current |
| 22 | A02 | /sessions [filter] | Lists sessions, accepts filter/search | Ignores filter |
| 23 | A03 | /stats [session_id] | Shows stats for specific session | Shows current session only |
| 24 | A04 | /new [name] | Creates new session with optional name | Ignores name |
| 25 | A05 | /undo [n] | Undoes N messages back | Ignores count, always one |
| 26 | A06 | /commands [category] | Lists commands filtered by category | Ignores filter |
| 27 | A07 | /tools [name] | Shows detail for specific tool | Ignores name, always lists all |
| 28 | A08 | /tools-verify [names] | Verifies specific tools | Ignores names, always all |
| 29 | A09 | /reset [confirm] | Resets with optional confirmation | Ignores confirm flag |
| 30 | A10 | /retry [n] | Retries last N messages | Ignores count, always last one |
| 31 | A11 | /compress [target] | Compresses to target token count | Ignores target |
| 32 | A12 | /status [session_id] | Shows status for specific session | Shows current only |
| 33 | A13 | /stop [name] | Stops specific process | Ignores name, stops all |
| 34 | A14 | /deny [reason] | Denies with reason | Ignores reason |
| 35 | A15 | /yolo [on/off] | Toggles or sets explicitly | Ignores arg, always toggles |
| 36 | A16 | /usage [session_id] | Shows usage for specific session | Shows current only |
| 37 | A17 | /plugins [name] | Shows detail for specific plugin | Ignores name |
| 38 | A18 | /platforms [filter] | Shows filtered platforms | Ignores filter |
| 39 | A19 | /verbose [level] | Sets verbosity level | Ignores level, always cycles |
| 40 | A20 | /profile [name] | Shows/activates named profile | Ignores profile name |
| 41 | A21 | /agents [detail] | Shows active agents with detail | **STUB: says "No active subagents"** |
| 42 | A22 | /toolsets [name] | Shows/activates specific toolset | Ignores name |
| 43 | A23 | /fast [on/off] | Sets fast mode explicitly | Ignores arg, always toggles |
| 44 | A24 | /copy [format] | Copies in specific format | Ignores format |
| 45 | A25 | /restart [force] | Graceful restart with drain | **STUB: says "Use /exit"** |
| 46 | A26 | /bundles [name] | Shows/activates specific bundle | Ignores name |
| 47 | A27 | /voice [on/off] | Voice mode toggle on/off/tts/status | Ignores mode |
| 48 | A28 | /statusbar [on/off] | Toggle statusbar | Ignores arg |
| 49 | A29 | /footer [on/off] | Toggle footer | Ignores arg |
| 50 | A30 | /update [branch] | Update to specific version/branch | Ignores branch |
| 51 | A31 | /debug [upload] | Uploads or shows debug info | Ignores upload target |
| 52 | A32 | /background [prompt] | Runs prompt in background | **STUB: says "not available"** |
| 53 | A33 | /dump [section] | Dumps specific section | Ignores section, dumps all |
| 54 | A34 | /config [key] [val] | Sets specific config key | Accepts key+val but behavior diverges |
| 55 | A35 | /model [name] [--provider] | Sets model with provider option | Ignores --provider flag |
| 56 | A36 | /topic [text] | Sets topic/personality | Accepts text |
| 57 | A37 | /personality [name] | Sets named personality | Accepts name |
| 58 | A38 | /insights [days] | Shows insights for N days | Accepts days but output is bare |
| 59 | A39 | /completions [shell] | Generates specific shell completions | Accepts shell name |
| 60 | A40 | /secrets [action] | Manages secrets with subcommand | Accepts action |

## SECTOR 0D: Missing Usages — Python Behavior Gaps (15 gaps)

Behaviors that work in Python but have NO equivalent in C, where a user would immediately say "this is not the same."

| # | ID | Usage | Python | C |
|---|-----|-------|--------|---|
| 61 | U01 | Config file format | YAML with full Python-style config sections (~60 keys) | C has ~20 config keys, many missing sections |
| 62 | U02 | Config editing | `/config key value` updates live config | Works but limited keys |
| 63 | U03 | Session auto-save | Auto-saves every N turns to SQLite | Auto-save present but behavior differs |
| 64 | U04 | Session resume | `/resume <id>` loads full state including tools/memory | Basic load, no tool state restore |
| 65 | U05 | Multi-line input | prompt_toolkit supports multi-line editing (Alt+Enter) | fgets() only — no multi-line |
| 66 | U06 | Error messages | Colored, formatted errors with suggestions | Plain printf errors |
| 67 | U07 | Tool progress modes | `/verbose` cycles: off→new→all→verbose with formatted output | Yes but output format differs |
| 68 | U08 | Profile switching | `--profile name` or `/profile name` switches config/credentials | Missing in C |
| 69 | U09 | Plugin dotfile loading | ~/.hermes/plugins/ auto-loads .so plugins at startup | plugin_ext.c exists but 0 plugins ship |
| 70 | U10 | Gateway per-platform config | Each platform has its own config section with auth/endpoint/timeout | Global config, no per-platform |
| 71 | U11 | MCP server config | Multiple MCP servers with different transports/auth | Basic single MCP config |
| 72 | U12 | Environment detection | Auto-detects terminal, Docker, SSH, WSL for display setup | Basic isatty() check |
| 73 | U13 | Tab completion for tools | Tools tab-complete their names in /tools <name> | Only commands tab-complete |
| 74 | U14 | Keyboard shortcuts | Ctrl+C interrupt, Ctrl+D exit, Ctrl+L clear, etc. | No keyboard shortcut handling |
| 75 | U15 | Gateway restart | `/restart` drains active connections, then restarts | Stub: "Use /exit" |

## SECTOR 1: P1 Critical Agent Modules (4 gaps)

| # | ID | Module | Python LOC | C State |
|---|-----|--------|-----------|---------|
| 76 | E01 | error_classifier | 1,087 | Inline string matching |
| 77 | E02 | chat_completion_helpers | 2,078 | Partial in llm_client.c |
| 78 | E03 | tool_executor | 910 | Inline in agent_loop.c |
| 79 | E04 | process_registry | 1,544 | Simpler process.c |

## SECTOR 2: Tool Depth — Function-Level Gaps (140 gaps)

**browser.c — 45 missing Python functions** (C: 15 funcs, Python: 75 funcs)
| # | ID | Area | Count |
|---|-----|------|-------|
| 80-124 | B01-B45 | browser_tool.py CDP/fill/nav/screenshot/cookie/network/emulation/forms/PDF/HAR | 45 funcs |

**vision.c — 18 missing** (C: 3 funcs, Python: 21 funcs)
| 125-142 | V01-V18 | OCR, face detect, barcode, EXIF, crop, rotate, analyze, classify | 18 funcs |

**web.c — 18 missing** (C: 6 funcs, Python: 24 funcs)
| 143-160 | W01-W18 | GET/POST/PUT/DELETE/HEAD/OPTIONS/PATCH, cookie jar, redirect, stream, rate-limit, proxy, retry, parse | 18 funcs |

**mcp_tool.c — 91 missing** (C: 18 funcs, Python: 109 funcs)
| 161-251 | M01-M91 | MCP protocol, transport, auth, subscriptions, sampling, notifications | 91 funcs |

**terminal.c — 44 missing** (C: 5 funcs, Python: 49 funcs)
| 252-295 | T01-T44 | Terminal env backends, timeout, signal handling, output buffering | 44 funcs |

**approval.c — 31 missing** (C: 11 funcs, Python: 42 funcs)
| 296-326 | A01-A31 | Approval policies, session approvals, deny reasons, expiry | 31 funcs |

**file.c — 16 missing** (C: 9 funcs, Python: 25 funcs)
| 327-342 | F01-F16 | Glob, fswatch, diff, hex view, type detection, symlink, permissions | 16 funcs |

**send_message.c + delegate.c — 71 missing**  
| 343-413 | S01-S39, D01-D32 | Platform routing, media handling, delegation features | 71 funcs |

## SECTOR 3: Gateway Platform Depth (19 gaps)

| # | ID | Platform | Missing Feature |
|---|-----|----------|----------------|
| 414 | G01 | telegram | Keyboard builder, poll support, inline queries |
| 415 | G02 | discord | Embed builder, thread management, modals |
| 416 | G03 | slack | Block builder, interactive components |
| 417 | G04 | signal | Attachment handling, reactions |
| 418 | G05 | whatsapp | Template system, buttons, lists |
| 419 | G06 | email | IMAP push, attachment parsing |
| 420 | G07 | matrix | Room discovery, edits, reactions |
| 421 | G08 | mattermost | File upload |
| 422 | G09 | feishu | Sheet ops, calendar, approvals |
| 423 | G10 | dingtalk | Card builder, interactive messages |
| 424 | G11 | wecom | Contact sync |
| 425 | G12 | weixin | Template management |
| 426 | G13 | qqbot | Voice channels |
| 427 | G14 | bluebubbles | iMessage reactions |
| 428 | G15 | homeassistant | History, long-lived tokens |
| 429 | G16 | webhook | HMAC verification, retry |
| 430 | G17 | sms | Delivery status |
| 431 | G18 | msgraph_webhook | Subscription renewal |
| 432 | G19 | yuanbao | All 4 sub-modules (media, proto, sticker) |

## SECTOR 4: Provider Feature Gaps (20 gaps)

| 433-452 | P01-P20 | Provider features: extended thinking, OAuth, rate-limit parsing, model discovery, error classification, credentials | 20 gaps |

## SECTOR 5: Agent Module Depth (14 gaps)

| 453-466 | M01-M14 | context_compressor, prompt_builder, memory_manager, insights, curator_backup, credential_sources, plugin_llm, skill_utils, background_review, stream_diag, agent_init, message_sanitization, conversation_compression | 14 gaps |

## SECTOR 6: Missing Python Tools — Full Module Ports (14 gaps)

| 467-480 | T01-T14 | browser_camofox, mcp_oauth, yuanbao_tools, microsoft_graph, credential_files, website_policy, osv_check, clarify_gateway, interrupt, env_passthrough, budget_config, schema_sanitizer, fuzzy_match, slash_confirm | 14 gaps |

## SECTOR 7: Gateway Sub-Modules (6 gaps)

| 481-486 | G20-G25 | feishu_comment, wecom_callback/crypto, yuanbao_media/proto/sticker, helpers/base | 6 gaps |

## SECTOR 8: Plugin System (4 gaps)

| 487-490 | PL01-PL04 | 29 model-provider plugins, 8 memory plugins, 10 other plugins, plugin LLM | 4 gaps |

## SECTOR 9: Library Depth (15 gaps)

| 491-505 | L01-L15 | JSON surrogate pairs, HTTP/2, retry, streaming, proxy, cookies, PKI encryption, session CRUD, LRU cache | 15 gaps |

## SECTOR 10: Config Key Gaps (8 gaps)

| 506-513 | C01-C08 | display.skin, agent.max_iterations, tools.browser_port, memory.provider, mcp.oauth, security.url_safety, gateway.platform_configs, auxiliary.vision_provider | 8 gaps |

## SECTOR 11: Test Coverage Gaps (5 gaps)

| 514-518 | TC01-TC05 | Gateway integration tests, CLI command tests, provider streaming tests, MCP transport tests, plugin loading tests | 5 gaps |

---

## Summary

| Sector | Category | Gaps |
|--------|----------|------|
| 0A | Entry Point Integration | 8 |
| 0B | Display & Visual Parity | 9 |
| 0C | CLI Behavioral Parity (args ignored) | 40 |
| 0D | Missing Usages (Python behavior gaps) | 15 |
| 1 | P1 Critical Agent Modules | 4 |
| 2 | Tool Depth (function-level) | 140 |
| 3 | Gateway Platform Depth | 19 |
| 4 | Provider Feature Gaps | 20 |
| 5 | Agent Module Depth | 14 |
| 6 | Missing Python Tools | 14 |
| 7 | Gateway Sub-Modules | 6 |
| 8 | Plugin System | 4 |
| 9 | Library Depth | 15 |
| 10 | Config Key Gaps | 8 |
| 11 | Test Coverage Gaps | 5 |
| **Total** | | **354** |

## Phase Order
1. **Phase 0a** — Entry Points ✅ (I01-I08)
2. **Phase 0b** — Display (V04-V12)
3. **Phase 0c** — CLI Behavioral (A01-A40: fix args handling on 40 commands)
4. **Phase 0d** — Missing Usages (U01-U15)
5. **Phase 1** — P1 Agent Modules (E01-E04)
6. **Phase 2** — Function-level tool/gateway/provider depth (S2-S5)
7. **Phase 3** — Missing ports + plugins (S6-S8)
8. **Phase 4** — Library/config/tests (S9-S11)
