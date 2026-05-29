# Prestige (v145)

## P0 — Display & Visual + Architecture (11 gaps)
D09, D16: Prompt input (partial), type-ahead
F01: C can't hook Python
F02: Test count mismatch (248 vs 1262)
F03: No Python interop
F04: Single-threaded vs asyncio
F09: No async event loop

## P1 — Critical Infrastructure (69 gaps)
L01-L23: Conversation loop plumbing (connection health, sanitization, image detection ✅, todo hydration, nudge counters, prefill injection, stream scrubber, think scrubber, memory triggers, skill triggers, compression warnings, fallback restore, auxiliary client, log tagging, skill origin, broken pipe guard, system prompt caching, compression feedback, budget tracking, error classification, runtime helpers, chat helpers, prompt builder, agent init)
A01-A20: Agent modules (conversation_loop, chat_completion_helpers, agent_runtime_helpers, anthropic_adapter, model_metadata, agent_init, prompt_builder, error_classifier, bedrock_adapter, google_oauth, display, gemini_native, tool_executor, memory_manager, conversation_compression, azure_identity, iteration_budget, system_prompt_builder, message_sanitization, nous_rate_guard, process_bootstrap)
T01-T14: TUI ecosystem (gateway server, transport, render, WebSocket, entry, slash worker, event publisher, 7 core tsx components)
X01-X09: Test coverage campaign
R01-R06: Provider adapter layer (anthropic, bedrock, google_oauth, gemini_native, azure_identity, model_metadata)
G01-G03: Gateway helpers (base, helpers, api_server)
F05, F08, F10: Credential automation, socket health, stdio guard
C11: Auth/OAuth system
P01: Plugin architecture

## P2 — Depth & Ecosystem (79 gaps)
A21-A34: Agent modules (codex_responses, plugin_llm, insights, models_dev, curator, title_generator, tracer, tool_result_classification, etc.)
L24-L28: Conversation loop depth (checkpoints, agent runtime, chat helpers, prompt builder, agent init)
C01-C17: CLI ecosystem (setup wizard, doctor, profiles, config, model management, gateway CLI, kanban, skills hub, etc.)
G04-G13: Gateway sub-modules (feishu_comment, wecom_crypto, telegram_network, etc.)
B01-B10: Tool depth (browser 42%, vision 14%, web 30%, mcp_tool 45%, file 46%, feishu_tools 24%, etc.)
T15-T24: TUI components (session picker, model picker, agents overlay, todo panel, etc.)
X10-X12: Performance/benchmark tests
R07-R10: Provider adapters (gemini_cloudcode, codex_responses, copilot_acp, plugin_llm)
P02-P05: Plugin ecosystem (memory providers, model providers, kanban, observability)
F06-F07: ACP protocol, session replay

## P3 — Polish & Niche (61 gaps)
Various small agent modules, plugin ecosystem, CLI edge cases, tool micro-features, fuzz tests

**Total: 207 structural gaps, 1000+ test case gaps — P0:7, P1:69, P2:79, P3:61**
