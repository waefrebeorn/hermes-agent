# Prestige (v255)

## P0 — Display & Visual + Architecture (6 gaps)
D09, D16: Prompt input (partial), type-ahead
F01: C can't hook Python
F02: Test count mismatch (272 vs 1262)
F03: No Python interop
F04: Single-threaded vs asyncio
F09: No async event loop

## P1 — Critical Infrastructure (37 gaps)
S1 L24-L28: Conversation loop depth (checkpoints, runtime helpers, chat helpers, prompt builder, agent init)
T01-T14: TUI Ecosystem (gateway server, transport, render, WebSocket, entry, slash worker, event publisher, 7 core tsx components)
X01-X09: Test coverage campaign
R01-R06: Provider adapter layer (anthropic, bedrock, google_oauth, gemini_native, azure_identity, model_metadata)
G01-G03: Gateway helpers (base, helpers, api_server)
F05, F08, F10: Credential automation, socket health, stdio guard
C11: Auth/OAuth system
P01: Plugin architecture

## P2 — Depth & Ecosystem (58 gaps)
C01-C17: CLI ecosystem (setup wizard, doctor, profiles, config, model management, gateway CLI, kanban, skills hub, etc.)
B01-B10: Tool depth (browser 60%, vision 39%, web ✅, mcp_tool ✅, file ✅, feishu ✅, terminal 40%, send_message 30%, patch ✅ 96%, session_search ✅ 96%)
G04-G13: Gateway sub-modules (feishu_comment ✅, wecom_crypto ✅, G06 wecom_callback PARTIAL — XML tag extraction + user_app_key ported)
T15-T24: TUI components (session picker, model picker, agents overlay, todo panel, etc.)
X10-X12: Performance/benchmark tests
R07-R10: Provider adapters (gemini_cloudcode, codex_responses, copilot_acp, plugin_llm)
P02-P05: Plugin ecosystem (memory providers, model providers, kanban, observability)
F06-F07: ACP protocol, session replay

## P3 — Polish & Niche (47 gaps)
Plugin ecosystem, CLI edge cases, tool micro-features, fuzz tests

**Total: 119 structural gaps, 1000+ test case gaps — P0:6, P1:36, P2:53, P3:24**
