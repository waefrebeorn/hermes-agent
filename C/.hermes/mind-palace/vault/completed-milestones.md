# Vault: Completed Milestones — Hermes C Translation

## Phase 1: Core Infrastructure (Completed)
- CLI framework: 148 commands, subcommand dispatch, JSON mode, pipe input, interactive REPL
- Config: 322+ YAML keys, env override, profiles (clone/delete), migration, validation, SIGHUP reload
- Agent loop: conversation loop, context management, streaming, reasoning, tool calling
- Provider adapters: OpenAI, Anthropic, DeepSeek, xAI, Google, Azure, Bedrock, OpenRouter, Custom
- MCP: 10/11 — SSE, HTTP, WebSocket transports, sampling handler, YAML config, dynamic server names
- ACP: 100% — JSON-RPC server, resource links, permissions, events, edit approval, plan updates
- Cron: 100% — scheduler, SQLite store, retry, templates, extras
- TUI: ncurses fullscreen terminal UI with session list, filtering
- Plugins: 10 .so (honcho, kanban, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet)

## Phase 2: Agent Modules (Completed)
| Module | Python LOC | Session | Status |
|--------|-----------|---------|--------|
| agent_loop | Core | N/A | ✅ Complete |
| llm_client | ~2500 | N/A | ✅ Complete |
| provider adapters (9) | ~500 each | N/A | ✅ Complete |
| tool_guardrails | 475 | S25 | ✅ Complete |
| i18n | 258 | S26 | ✅ Complete |
| subdir_hints | 224 | S27 | ✅ Complete |
| onboarding | 193 | S28 | ✅ Complete |
| error_classifier | 1134 | S52 | ✅ Complete |
| image_routing | 391 | S53 | ✅ Complete |
| tool_dispatch_helpers | 350 | S36 | ✅ Ported to libtooldispatch |
| rate_guard | 325 | S50 | ✅ Ported to librateguard |
| rate_limit_tracker | ~200 | N/A | ✅ as libratelimit |
| credential_pool | ~300 | N/A | ✅ Complete |
| budget_tracker | ~150 | N/A | ✅ Complete |
| fallback_routing | ~250 | N/A | ✅ Complete |
| checkpoint | ~200 | N/A | ✅ Complete |
| context | ~400 | N/A | ✅ Complete |
| system_prompt | ~300 | N/A | ✅ Complete |
| audit | ~150 | N/A | ✅ Complete |
| retry_utils | ~100 | N/A | ✅ Complete |
| vault | ~200 | N/A | ✅ Complete |
| redact | ~300 | N/A | ✅ Complete |
| sanitize | ~200 | N/A | ✅ Complete |
| think_scrubber | ~100 | N/A | ✅ Complete |
| trajectory | ~200 | N/A | ✅ Complete |
| title | ~100 | N/A | ✅ Complete |
| file_safety | ~200 | N/A | ✅ Complete |
| skill_preprocessing | ~200 | N/A | ✅ Complete |
| portal_tags | ~100 | N/A | ✅ Complete |
| markdown_tables | ~150 | N/A | ✅ Complete |

## Phase 3: Tool Ports (Completed)
| Tool | C File | Lines | Status |
|------|--------|:-----:|--------|
| terminal | terminal.c | ~800 | ✅ Complete |
| read/write/search files | file.c | ~900 | ✅ Complete |
| web_get/search/extract | web.c | ~500 | ✅ Complete |
| skill CRUD + hub | skills.c, skill_mgmt.c | ~1500 | ✅ Complete |
| patch | patch.c | ~400 | ✅ Complete |
| execute_code | exec_code.c | ~300 | ✅ Complete |
| clarify | clarify.c | ~300 | ✅ Complete |
| memory | memory.c | ~400 | ✅ Complete |
| todo | todo.c | ~400 | ✅ Complete |
| process | process.c | ~1100 | ✅ Complete |
| send_message | send_message.c | ~200 | ✅ Complete |
| cronjob | cronjob.c | ~300 | ✅ Complete |
| session_search | session_search.c | ~400 | ✅ Complete |
| session_crud | session_crud.c | ~200 | ✅ Complete |
| tts | tts.c | ~500 | ✅ Complete |
| vision_analyze | vision.c | ~200 | ✅ Complete |
| delegate_task | delegate.c | ~300 | ✅ Complete |
| x_search | x_search.c | ~400 | ✅ Complete |
| browser | browser.c | ~1500 | ✅ Complete (except CDP stub) |
| approval | approval.c | ~400 | ✅ Complete |
| voice_mode | voice_mode.c | ~600 | ✅ Complete |
| image_gen | image_gen.c | ~400 | ✅ Complete |
| homeassistant | homeassistant.c | ~600 | ✅ Complete |
| kanban | kanban.c | ~800 | ✅ Complete |
| computer_use | computer_use.c | ~1300 | ✅ Complete |
| mcp_tool | mcp_tool.c | ~1300 | ✅ Complete |
| video_gen | video_gen.c | ~300 | ✅ Complete |
| discord | discord.c | ~200 | ✅ Complete |
| file_batch | file_batch.c | ~300 | ✅ Complete |
| url_safety | url_safety.c | ~200 | ✅ Complete |
| tirith | tirith.c | ~200 | ✅ Complete |
| api_helpers | api_helpers.c | ~200 | ✅ Complete |
| tool_config | tool_config.c | ~200 | ✅ Complete |
| rate_limit | rate_limit.c | ~100 | ✅ Complete |
| result_storage | result_storage.c | ~200 | ✅ Complete |

## Phase 4: DA Audits (Completed)
- DA v11 (May 23, Session ~30): ~32% parity, ~500 gap battleship
- DA v13 (Jun 1, Session 51): ~60% parity, all stubs resolved, ~300/500
- DA v14 (May 23, Session 53): ~65% parity, 1 stub (browser_cdp), 12 verified major gaps
