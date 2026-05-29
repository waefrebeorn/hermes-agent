# Prestige (v145)

## P0 — Display & Visual + Architecture (16 gaps)
D01-D12: Skin engine, spinner, banner, status bar, tool feed, response box, help tables, 24-bit color, prompt input, markdown render, faces, emoji
F01: C can't hook Python
F02: Test count mismatch (248 vs 1262)
F03: No Python interop
F04: Single-threaded vs asyncio

## P1 — Critical Infrastructure (53 gaps)
A01-A18: Agent modules (conversation_loop, chat_completion_helpers, agent_runtime_helpers, 4 adapter layers, etc.)
T01-T14: TUI ecosystem (gateway server, transport, render, 11 core tsx components)
X01-X09: Test coverage campaign (files, LOC, provider tests, tool tests, integration tests, etc.)
R01-R06: Provider adapter layer (anthropic, bedrock, google_oauth, gemini_native, azure_identity, model_metadata)
G01-G03: Gateway helpers (base, helpers, api_server)
F05: No credential automation
C11: Auth/OAuth system
P01: Plugin architecture

## P2 — Depth & Ecosystem (76 gaps)
A19-A34: Agent modules (models_dev, copilot, memory_manager, conversation_compression, background_review, etc.)
C01-C17: CLI ecosystem (setup wizard, doctor, profiles, config, model management, gateway CLI, kanban system, skills hub, etc.)
S2 G04-G13: Gateway sub-modules (feishu_comment, wecom_crypto, telegram_network, etc.)
B01-B10: Tool depth (browser, vision, web, mcp_tool, file, feishu_tools, terminal, send_message, patch, session_search)
T15-T24: TUI components (session picker, model picker, agents overlay, todo panel, etc.)
X10-X12: Performance/benchmark tests
R07-R10: Provider adapters (gemini_cloudcode, codex_responses, copilot_acp, plugin_llm)
P02-P05: Plugin ecosystem (memory providers, model providers, kanban, observability)
F06-F07: ACP protocol, session replay

## P3 — Polish & Niche (60 gaps)
Various small agent modules, plugin ecosystem, CLI edge cases, tool micro-features

**Total: 205+ structural gaps, 1000+ test case gaps — P0:16, P1:53, P2:76, P3:60**
