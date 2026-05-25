# Slermes C — Vault: Hermes Upstream Update

## Python Hermes Upstream Status (as of 2026-05-25)

The Python Hermes codebase at `/home/wubu/hermes-agent-dev/` has advanced independently since the C translation was forked. Below are the upstream modules/features with NO C equivalent in `C/`.

### New Python Agent Modules (not in C)
- `agent/agent_init.py` (1,522 LOC) — Agent initialization orchestration
- `agent/agent_runtime_helpers.py` (350 LOC) — Runtime helper functions
- `agent/anthropic_adapter.py` (2,220 LOC) — Anthropic API protocol adapter
- `agent/async_utils.py` (68 LOC) — Async utility functions
- `agent/azure_identity_adapter.py` (555 LOC) — Azure Entra ID keyless auth
- `agent/background_review.py` (582 LOC) — Background tool result review
- `agent/bedrock_adapter.py` (1,289 LOC) — AWS Bedrock Converse API
- `agent/browser_provider.py` (175 LOC) — Browser provider abstraction
- `agent/browser_registry.py` (223 LOC) — Browser provider registry
- `agent/chat_completion_helpers.py` (2,078 LOC) — Message/token helpers
- `agent/codex_responses_adapter.py` (1,084 LOC) — Codex Responses API
- `agent/codex_runtime.py` (448 LOC) — Codex app-server runtime
- `agent/context_compressor.py` (1,748 LOC) — Adaptive compression
- `agent/conversation_compression.py` (603 LOC) — Conversation compression
- `agent/copilot_acp_client.py` (686 LOC) — GitHub Copilot ACP
- `agent/credential_sources.py` (448 LOC) — Multi-source credentials
- `agent/curator_backup.py` (693 LOC) — Skill backup/rollback
- `agent/display.py` (987 LOC) — KawaiiSpinner + display helpers
- `agent/error_classifier.py` (1,087 LOC) — API error taxonomy
- `agent/gemini_cloudcode_adapter.py` (909 LOC) — Gemini Cloud Code
- `agent/gemini_native_adapter.py` (971 LOC) — Gemini native API
- `agent/google_code_assist.py` (452 LOC) — Google Code Assist
- `agent/google_oauth.py` (1,061 LOC) — Google OAuth PKCE flow
- `agent/image_gen_provider.py` (242 LOC) — Image generation provider
- `agent/insights.py` (930 LOC) — Usage insights engine
- `agent/memory_manager.py` (609 LOC) — Multi-provider memory
- `agent/memory_provider.py` (279 LOC) — Memory provider interface
- `agent/message_sanitization.py` (444 LOC) — Output sanitization
- `agent/model_metadata.py` (1,827 LOC) — Full model database
- `agent/models_dev.py` (723 LOC) — Remote model discovery
- `agent/plugin_llm.py` (1,046 LOC) — LLM plugin calls
- `agent/process_bootstrap.py` (167 LOC) — Background process bootstrap
- `agent/prompt_builder.py` (1,465 LOC) — Advanced prompt construction
- `agent/rate_limit_tracker.py` (246 LOC) — Rate limit header parsing
- `agent/skill_utils.py` (511 LOC) — Skill utility functions
- `agent/stream_diag.py` (280 LOC) — Stream diagnostic tracing
- `agent/title_generator.py` (189 LOC) — Auto-title generation (C has title.c)
- `agent/tool_dispatch_helpers.py` (350 LOC) — Tool dispatch utilities
- `agent/tool_executor.py` (910 LOC) — Tool execution orchestration
- `agent/video_gen_provider.py` (299 LOC) — Video generation provider
- `agent/web_search_provider.py` (221 LOC) — Web search provider

### New Python Tools (not in C)
- 14 full tool modules (listed in battleship S6)

### New Python Gateway Platforms help modules
- 6 sub-modules (feishu_comment, wecom_callback/crypto, yuanbao_media/proto/sticker, helpers/base)

### Python Plugin System
- 29 model-provider plugins
- 8 memory provider plugins
- 10+ other plugins (browser, obs, kanban, achievements, etc.)

## Upstream Extension Roadmap

The following Python modules represent significant NEW functionality that exists only in Python after the C fork. Each one needs a C port:

**Tier 1 (Critical for parity):**
1. `error_classifier.py` — Structured error handling
2. `chat_completion_helpers.py` — Message formatting
3. `context_compressor.py` — Adaptive compression
4. `prompt_builder.py` — Template system
5. `display.py` — Visual parity (Phase 0b)

**Tier 2 (Important):**
6. `insights.py` — Usage analytics
7. `memory_manager.py` — Multi-provider memory
8. `model_metadata.py` — Model database
9. `plugin_llm.py` — Plugin LLM
10. `curator_backup.py` — Backup system

**Tier 3 (Nice to have):**
11-41. Remaining 31 Python modules

All upstream gaps are captured in battleship-v10 Sectors 2-8.
