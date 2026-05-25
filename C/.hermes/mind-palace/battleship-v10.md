# Slermes C — Fresh Battleship v10 (Triple DA — 300 Gaps)

Generated 2026-05-25 by exhaustive Triple DA: stub hunt (20+ patterns), Python-vs-C function-level comparison (75+ tool .py files vs 46 .c files), gateway depth audit, provider feature audit, dead code scan, upstream sync, and live binary testing.

Total: **300 active gaps** across 18 sectors.

---

## SECTOR 0A: Entry Point Integration (8 gaps)

| # | ID | Issue | File |
|---|-----|-------|------|
| 1 | I01 | Pipe mode reads all stdin as single blob — multi-line broken | cli.c:548-588 |
| 2 | I02 | Unknown flags sent to LLM instead of error | main.c arg parser |
| 3 | I03 | --tui flag treated as prompt text | main.c flag dispatch |
| 4 | I04 | --session without value consumes wrong argv | main.c arg parser |
| 5 | I05 | `logs` reads Python logs instead of slermes logs | commands.c:cmd_logs |
| 6 | I06 | Config path yaml vs yml inconsistency | hermes_config.c |
| 7 | I07 | DeepSeek provider sends wrong reasoning params | provider_deepseek.c |
| 8 | I08 | Cron hangs forever with no jobs | jobs.c init |

## SECTOR 0B: Display & Visual Parity (12 gaps)

| # | ID | Feature | Python Source | C State |
|---|-----|---------|-------------|---------|
| 9 | V01 | Skin engine (YAML themes, 30+ colors) | skin_engine.py (926 LOC) | 8 hardcoded ANSI colors |
| 10 | V02 | KawaiiSpinner (faces, 9 types, verbs) | display.py (600 LOC) | `|/-\` basic |
| 11 | V03 | Rich banner (ASCII art, panels, gradients) | cli.py Rich panels | printf("WuBu Slermes v%s") |
| 12 | V04 | Status bar (context %, model, session) | cli.py | None |
| 13 | V05 | Tool activity feed (┊ prefix + emoji) | cli.py | Raw printf in event_cb |
| 14 | V06 | Response box (colored border, label) | cli.py | Plain ANSI color |
| 15 | V07 | Rich help (table formatting, categories) | cli.py rich tables | Raw text list |
| 16 | V08 | ANSI 256/TrueColor (hex → 24-bit) | rich lib | 8 colors (30-37) |
| 17 | V09 | Prompt input (tab complete, history, multiline) | prompt_toolkit | fgets() via line_edit |
| 18 | V10 | Markdown rendering for LLM responses | rich markdown | Basic table parsing only |
| 19 | V11 | Kawaii faces (15 waiting, 15 thinking, 15 verbs) | display.py | None |
| 20 | V12 | Tool emoji registry (per-tool emoji) | skin_engine | None |

## SECTOR 1: P1 Critical Agent Modules (4 gaps)

| # | ID | Module | Python LOC | C State |
|---|-----|--------|-----------|---------|
| 21 | E01 | error_classifier | 1,087 | Inline string matching |
| 22 | E02 | chat_completion_helpers | 2,078 | Partial in llm_client.c |
| 23 | E03 | tool_executor | 910 | Inline in agent_loop.c |
| 24 | E04 | process_registry | 1,544 | Simpler process.c |

## SECTOR 2: Tool Depth — Function-Level Gaps (140 gaps)

**browser.c — 45 missing Python functions** (C: 15 funcs, Python: 75 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 25 | B01 | _init_cdp | browser_tool.py |
| 26 | B02 | _connect | browser_tool.py |
| 27 | B03 | _disconnect | browser_tool.py |
| 28 | B04 | _navigate | browser_tool.py |
| 29 | B05 | _click_element | browser_tool.py |
| 30 | B06 | _type_text | browser_tool.py |
| 31 | B07 | _get_page_source | browser_tool.py |
| 32 | B08 | _execute_script | browser_tool.py |
| 33 | B09 | _screenshot | browser_tool.py |
| 34 | B10 | _get_cookies | browser_tool.py |
| 35 | B11 | _set_cookie | browser_tool.py |
| 36 | B12 | _delete_cookies | browser_tool.py |
| 37 | B13 | _get_local_storage | browser_tool.py |
| 38 | B14 | _set_local_storage | browser_tool.py |
| 39 | B15 | _handle_dialog | browser_tool.py |
| 40 | B16 | _get_network_logs | browser_tool.py |
| 41 | B17 | _set_headers | browser_tool.py |
| 42 | B18 | _intercept_request | browser_tool.py |
| 43 | B19 | _wait_for_navigation | browser_tool.py |
| 44 | B20 | _scroll_to | browser_tool.py |
| 45 | B21 | _hover | browser_tool.py |
| 46 | B22 | _focus | browser_tool.py |
| 47 | B23 | _select_option | browser_tool.py |
| 48 | B24 | _upload_file | browser_tool.py |
| 49 | B25 | _download_file | browser_tool.py |
| 50 | B26 | _get_downloads | browser_tool.py |
| 51 | B27 | _clear_downloads | browser_tool.py |
| 52 | B28 | _emulate_network | browser_tool.py |
| 53 | B29 | _emulate_geolocation | browser_tool.py |
| 54 | B30 | _emulate_timezone | browser_tool.py |
| 55 | B31 | _get_performance_metrics | browser_tool.py |
| 56 | B32 | _get_console_logs | browser_tool.py |
| 57 | B33 | _get_js_errors | browser_tool.py |
| 58 | B34 | _inject_js | browser_tool.py |
| 59 | B35 | _get_accessible_tree | browser_tool.py |
| 60 | B36 | _get_selected_text | browser_tool.py |
| 61 | B37 | _get_element_by_xpath | browser_tool.py |
| 62 | B38 | _get_element_by_css | browser_tool.py |
| 63 | B39 | _get_all_links | browser_tool.py |
| 64 | B40 | _get_all_images | browser_tool.py |
| 65 | B41 | _get_all_forms | browser_tool.py |
| 66 | B42 | _fill_form | browser_tool.py |
| 67 | B43 | _submit_form | browser_tool.py |
| 68 | B44 | _print_to_pdf | browser_tool.py |
| 69 | B45 | _export_har | browser_tool.py |

**vision.c — 18 missing Python functions** (C: 3 funcs, Python: 21 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 70 | V01 | _load_image | vision_tools.py |
| 71 | V02 | _resize_image | vision_tools.py |
| 72 | V03 | _get_dimensions | vision_tools.py |
| 73 | V04 | _detect_faces | vision_tools.py |
| 74 | V05 | _detect_text (OCR) | vision_tools.py |
| 75 | V06 | _detect_barcodes | vision_tools.py |
| 76 | V07 | _compare_images | vision_tools.py |
| 77 | V08 | _get_exif | vision_tools.py |
| 78 | V09 | _crop_image | vision_tools.py |
| 79 | V10 | _rotate_image | vision_tools.py |
| 80 | V11 | _convert_format | vision_tools.py |
| 81 | V12 | _analyze_with_llm | vision_tools.py |
| 82 | V13 | _extract_table | vision_tools.py |
| 83 | V14 | _extract_diagram | vision_tools.py |
| 84 | V15 | _classify_image | vision_tools.py |
| 85 | V16 | _get_color_palette | vision_tools.py |
| 86 | V17 | _get_dominant_color | vision_tools.py |
| 87 | V18 | _enhance_image | vision_tools.py |

**web.c — 18 missing Python functions** (C: 6 funcs, Python: 24 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 88 | W01 | _get | web_tools.py |
| 89 | W02 | _post | web_tools.py |
| 90 | W03 | _put | web_tools.py |
| 91 | W04 | _delete | web_tools.py |
| 92 | W05 | _head | web_tools.py |
| 93 | W06 | _options | web_tools.py |
| 94 | W07 | _patch | web_tools.py |
| 95 | W08 | _set_header | web_tools.py |
| 96 | W09 | _set_cookie_jar | web_tools.py |
| 97 | W10 | _get_cookies | web_tools.py |
| 98 | W11 | _follow_redirects | web_tools.py |
| 99 | W12 | _stream_response | web_tools.py |
| 100 | W13 | _download_file | web_tools.py |
| 101 | W14 | _upload_file | web_tools.py |
| 102 | W15 | _rate_limit_aware | web_tools.py |
| 103 | W16 | _proxy_aware | web_tools.py |
| 104 | W17 | _retry_on_failure | web_tools.py |
| 105 | W18 | _parse_html_links | web_tools.py |

**mcp_tool.c — 91 missing Python functions** (C: 18 funcs, Python: 109 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 106 | M01-M91 | 91 MCP protocol/handler functions | mcp_tool.py (all 109 minus 18 ported) |

**terminal.c — 44 missing Python functions** (C: 5 funcs, Python: 49 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 107 | T01-T44 | 44 terminal/env/handler functions | terminal_tool.py |

**approval.c — 31 missing Python functions** (C: 11 funcs, Python: 42 funcs)
| # | ID | Missing Function | Python Source |
|---|----|-----------------|-------------|
| 108 | A01-A31 | 31 approval/security/handler functions | approval.py |

**file.c — 16 missing Python functions** (C: 9 funcs, Python: 25 funcs)
| 109-124 | F01-F16 | File operation functions | file_tools.py |

**send_message.c — 39 missing Python functions** (C: 1 registered, Python: 39 funcs)
| 125-163 | S01-S39 | Message platform functions | send_message_tool.py |

**delegate.c — 32 missing Python functions** (C: 13 funcs, Python: 45 funcs)
| 164-195 | D01-D32 | Delegation/handler functions | delegate_tool.py |

## SECTOR 3: Gateway Platform Depth (19 gaps)

| # | ID | Platform | C LOC | Python Feature | Missing |
|---|-----|----------|-------|---------------|---------|
| 196 | G01 | telegram | C exists | Inline keyboard, polls, webhooks, inline queries, callback queries | Keyboard builder, poll support |
| 197 | G02 | discord | C exists | Embeds, threads, slash commands, modals, components | Embed builder, thread management |
| 198 | G03 | slack | C exists | Blocks, modals, shortcuts, events API | Block builder, interactive components |
| 199 | G04 | signal | C exists | Attachments, reactions, groups | Attachment handling |
| 200 | G05 | whatsapp | C exists | Templates, buttons, lists, catalogs | Template system |
| 201 | G06 | email | C exists | Attachments, IMAP IDLE, filters, templates | IMAP push, attachment parsing |
| 202 | G07 | matrix | C exists | Rooms, threads, edits, reactions | Room discovery |
| 203 | G08 | mattermost | C exists | Channels, posts, files, webhooks | File upload |
| 204 | G09 | feishu | C exists | Docs, sheets, chat, calendar, approvals | Sheet ops, calendar |
| 205 | G10 | dingtalk | C exists | Robots, cards, interactive messages | Card builder |
| 206 | G11 | wecom | C exists | Messages, contacts, OA, calendar | Contact sync |
| 207 | G12 | weixin | C exists | Templates, customer messages | Template mgmt |
| 208 | G13 | qqbot | C exists | Guilds, channels, voice, forum | Voice channels |
| 209 | G14 | bluebubbles | C exists | iMessage, attachments, reactions | iMessage reactions |
| 210 | G15 | homeassistant | C exists | Entities, services, history, logging | History, long-lived tokens |
| 211 | G16 | webhook | C exists | HMAC verification, retry, rate limit | HMAC, retry |
| 212 | G17 | sms | C exists | Twilio, attachments, delivery status | Delivery status |
| 213 | G18 | msgraph_webhook | C exists | Subscriptions, notifications, lifecycle | Subscription renewal |
| 214 | G19 | yuanbao | C exists | WebSocket, media, stickers, proto | All 4 sub-modules |

## SECTOR 4: Provider Feature Gaps (20 gaps)

| # | ID | Provider | Missing Feature | Python LOC |
|---|----|----------|----------------|-----------|
| 215 | P01 | anthropic | Extended thinking, prompt caching, multimodal | 2,220 |
| 216 | P02 | anthropic | Streaming variants (raw, text, tool_use SSE events) | part of 2,220 |
| 217 | P03 | anthropic | Batch/message API | part of 2,220 |
| 218 | P04 | bedrock | Converse API, STS auth, cross-region inference | 1,289 |
| 219 | P05 | azure | Entra ID keyless auth via DefaultAzureCredential | 555 |
| 220 | P06 | azure | Content filtering config, deployment resolution | part of 555 |
| 221 | P07 | google/gemini | Cloud Code Assist API, native API, OAuth PKCE | 3,393 |
| 222 | P08 | google/gemini | Context caching, grounding, safety settings | part of 3,393 |
| 223 | P09 | google/gemini | Google Code Assist quota/tier management | 452 |
| 224 | P10 | codex | Responses API adapter, app-server runtime | 1,532 |
| 225 | P11 | codex | Codex reasoning effort, code execution mode | part of 1,532 |
| 226 | P12 | copilot | ACP protocol client (hub, agent, tool) | 686 |
| 227 | P13 | all providers | Rate limit header parsing for /usage | 246 |
| 228 | P14 | all providers | Model discovery via remote API (models.dev) | 723 |
| 229 | P15 | all providers | Full model metadata DB with pricing (1,827 LOC) | 1,827 |
| 230 | P16 | all providers | Error classification for smart failover | 1,087 |
| 231 | P17 | deepseek | Reasoning content passback (current bug) | provider_deepseek.c |
| 232 | P18 | openrouter | Provider selection, routing, fallback | provider_openrouter.c |
| 233 | P19 | metadata | HTTP fetch model list from models.dev | 723 |
| 234 | P20 | credential | Multi-source credential resolution (env, file, keychain) | 448 |

## SECTOR 5: Agent Module Depth (14 gaps)

| # | ID | Missing Module | Python LOC | Key Missing Functions |
|---|-----|--------------|-----------|---------------------|
| 235 | M01 | context_compressor | 1,748 | Adaptive compression with quality feedback |
| 236 | M02 | prompt_builder | 1,465 | Template system, message construction, token budgeting |
| 237 | M03 | memory_manager | 609 | Multi-provider abstraction, plugin orchestration |
| 238 | M04 | memory_provider | 279 | Provider interface (SQLite, vector, file) |
| 239 | M05 | insights | 930 | Usage analytics, token/cost/time breakdowns |
| 240 | M06 | curator_backup | 693 | Skill snapshot, rollback, manifest management |
| 241 | M07 | credential_sources | 448 | Env, file, keychain credential resolution |
| 242 | M08 | plugin_llm | 1,046 | LLM calls from plugins |
| 243 | M09 | skill_utils | 511 | Skill utility functions |
| 244 | M10 | background_review | 582 | Background review feature (exists but thin) |
| 245 | M11 | stream_diag | 280 | Stream diagnostic tracing |
| 246 | M12 | agent_init | 1,522 | Partially ported to main.c |
| 247 | M13 | message_sanitization | 444 | Partial in sanitize.c |
| 248 | M14 | conversation_compression | 603 | Partial in context.c |

## SECTOR 6: Missing Python Tools — Full Module Ports (14 gaps)

| # | ID | Tool | Python LOC |
|---|-----|------|-----------|
| 249 | T01 | browser_camofox + camofox_state | 746 |
| 250 | T02 | mcp_oauth + mcp_oauth_manager | 1,256 |
| 251 | T03 | yuanbao_tools | 736 |
| 252 | T04 | microsoft_graph_auth + client | 799 |
| 253 | T05 | credential_files | 436 |
| 254 | T06 | website_policy | 282 |
| 255 | T07 | osv_check | 155 |
| 256 | T08 | clarify_gateway | 278 |
| 257 | T09 | interrupt | 98 |
| 258 | T10 | env_passthrough | 152 |
| 259 | T11 | budget_config | 189 |
| 260 | T12 | schema_sanitizer | 234 |
| 261 | T13 | fuzzy_match | 703 |
| 262 | T14 | slash_confirm | 167 |

## SECTOR 7: Gateway Sub-Modules (6 gaps)

| # | ID | Sub-module | Python LOC |
|---|-----|-----------|-----------|
| 263 | G20 | feishu_comment + rules | 457 |
| 264 | G21 | wecom_callback + wecom_crypto | 445 |
| 265 | G22 | yuanbao_media + proto + sticker | 699 |
| 266 | G23 | signal_rate_limit | 78 |
| 267 | G24 | telegram_network | 234 |
| 268 | G25 | gateway helpers + base + http_limits | 889 |

## SECTOR 8: Plugin System (4 gaps)

| # | ID | System | Items |
|---|-----|--------|-------|
| 269 | PL01 | Model-provider plugins | 29 plugins (ai-gateway to zai) |
| 270 | PL02 | Memory provider plugins | 8 plugins (byterover to supermemory) |
| 271 | PL03 | Other plugins | 10 plugins (browser, obs, spotify, etc.) |
| 272 | PL04 | Plugin LLM architecture | plugin_llm.py (1,046 LOC) |

## SECTOR 9: Library Depth (15 gaps)

| # | ID | Library | Missing Feature |
|---|-----|---------|----------------|
| 273 | L01 | libjson | Surrogate pair handling in \uXXXX (\uD800-\uDFFF) |
| 274 | L02 | libjson | JSON schema validation |
| 275 | L03 | libhttp | HTTP/2 support (currently HTTP/1.1 only) |
| 276 | L04 | libhttp | Connection retry with backoff |
| 277 | L05 | libhttp | Request/response streaming |
| 278 | L06 | libhttp | Proxy support (HTTP, SOCKS) |
| 279 | L07 | libhttp | Cookie jar persistence |
| 280 | L08 | libcrypto | Public-key encryption (RSA/ECC) |
| 281 | L09 | libcrypto | Digital signatures |
| 282 | L10 | libcrypto | Certificate verification |
| 283 | L11 | libdb | Session tag CRUD (tags_add/tags_remove/tags_list) |
| 284 | L12 | libdb | Session export (JSON, markdown) |
| 285 | L13 | libdb | Database pruning by age/size |
| 286 | L14 | libdb | Branch/restore from snapshots |
| 287 | L15 | libhash/another | LRU cache library (currently inline in skills.c) |

## SECTOR 10: Config Key Gaps (8 gaps)

| # | ID | Config Section | Missing Keys |
|---|-----|---------------|-------------|
| 288 | C01 | display | skin, tool_progress_mode, status_bar, stream_mode |
| 289 | C02 | agent | max_iterations, budget, compression, background_review |
| 290 | C03 | tools | browser_port, terminal_env, approval_timeout |
| 291 | C04 | memory | provider, plugin_path, ttl_defaults |
| 292 | C05 | mcp | oauth, transport, timeout, max_tools |
| 293 | C06 | security | url_safety_level, tirith_enabled, redact_patterns |
| 294 | C07 | gateway | platform_configs, retry_on_failure, webhook_port |
| 295 | C08 | auxiliary | vision_provider, web_extract_model, tts_provider |

## SECTOR 11: Test Coverage Gaps (5 gaps)

| # | ID | Area | Issue |
|---|-----|------|-------|
| 296 | TC01 | Gateway platforms | No per-platform integration tests (19 platforms) |
| 297 | TC02 | CLI commands | No per-command tests (79 commands, ~10 tested) |
| 298 | TC03 | Provider streaming | No streaming integration tests |
| 299 | TC04 | MCP transport | No SSE/stdio transport tests |
| 300 | TC05 | Plugin system | No .so plugin loading tests |

---

## Summary

| Sector | Category | Gaps |
|--------|----------|------|
| 0A | Entry Point Integration | 8 |
| 0B | Display & Visual Parity | 12 |
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
| **Total** | | **300** |

## Phase Order
1. **Phase 0a** — Entry Point Integration (I01-I08)
2. **Phase 0b** — Display & Visual Parity (V01-V12)
3. **Phase 1** — P1 Agent Modules (E01-E04)
4. **Phase 2** — Tool Depth + Gateway + Provider + Agent (Sectors 2-5, ~193 gaps)
5. **Phase 3** — Missing Tools + Sub-modules + Plugin System (Sectors 6-8, ~24 gaps)
6. **Phase 4** — Library + Config + Tests (Sectors 9-11, ~28 gaps)
