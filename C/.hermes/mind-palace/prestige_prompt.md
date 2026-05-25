# Slermes C — Prestige Prompt (v3 — 2026-05-25)

## Priority Queue — 67 Real Gaps (Phase Order)

### Phase 0 — Display & Visual Parity (P0 — 12 gaps)
**Do FIRST.** Users experience parity through CLI visuals first. Must match Python Hermes 1:1.

1. **V01 — Skin Engine** — Port `hermes_cli/skin_engine.py` (926 LOC). YAML-based theme system with 30+ color configs, spinner customization, branding, tool emojis. C has 8 hardcoded ANSI colors.
2. **V02 — KawaiiSpinner** — Port `agent/display.py` (600 LOC). Animated kawaii faces (｡◕‿◕｡), 15 thinking faces, 15 verbs, 9 spinner types (dots/bounce/grow/arrows/star/moon/pulse/brain/sparkle). C has `|/-\`.
3. **V03 — Rich Banner** — Port Python Rich banner. ASCII art + colored panels with gold borders, section headers, tool list, branding. C has `printf("WuBu Slermes v%s\n")`.
4. **V04 — Status Bar** — Port Python status bar. Context usage %, model name, session info, color-coded warnings. C has nothing.
5. **V05 — Tool Activity Feed** — Port `┊` prefix + tool emoji for every tool call output. C tools emit raw printf.
6. **V06 — Response Box** — Port colored response panel with label (╔═ ═╗). C has `printf()` with basic ANSI color.
7. **V07 — Rich Help** — Port Rich table formatting for `/help`. C has raw text list. Add category grouping, color headers.
8. **V08 — ANSI 256/TrueColor** — Extend from 8 ANSI colors to 24-bit truecolor (hex `#RRGGBB`). Port color space mapping from `rich`.
9. **V09 — Prompt Input** — Port `prompt_toolkit` features: tab completion, history search, multi-line editing, syntax highlight. C uses `fgets()`.
10. **V10 — Markdown Rendering** — Port Rich markdown renderer for LLM responses. C has `markdown_tables.c` but not full markdown.
11. **V11 — Kawaii Faces** — Embed 15 kawaii waiting faces + 15 thinking faces + 15 thinking verbs + 9 spinner styles from Python.
12. **V12 — Tool Emoji Registry** — Port tool emoji mapping from skin engine per tool name.

### 🔴 Phase 1 — P1 Agent Modules (4 gaps)
13. **E01 — error_classifier** — Port agent/error_classifier.py (1,087 LOC)
14. **E02 — chat_completion_helpers** — Port agent/chat_completion_helpers.py (2,078 LOC)
15. **E03 — tool_executor** — Restructure tool dispatch (910 LOC)
16. **E04 — process_registry** — Port tools/process_registry.py (1,544 LOC)

### 🟡 Phase 2 — P2 Ports (33 gaps)
17-30. **Missing Tools (14)** — browser_camofox, mcp_oauth, yuanbao_tools, microsoft_graph, credential_files, website_policy, osv_check, clarify_gateway, interrupt, env_passthrough, budget_config, schema_sanitizer, fuzzy_match, slash_confirm
31-37. **Provider Adapters (7)** — anthropic_adapter, bedrock_adapter, azure_identity, gemini_cloudcode, gemini_native, google_oauth, codex_responses
38-43. **Gateway Sub-modules (6)** — feishu_comment, wecom_callback/crypto, yuanbao_media/proto/sticker, signal_rate_limit, telegram_network, helpers/base
44-49. **Agent Modules (6)** — insights, memory_manager, rate_limit_tracker, models_dev, curator_backup, context_compressor

### 🟢 Phase 3 — P3 Depth (18 gaps)
50-56. **Missing Modules (7)** — credential_sources, plugin_llm, model_metadata, prompt_builder, skill_utils, background_review, stream_diag
57-63. **Tool Depth (7)** — browser (42%), vision (14%), web (30%), mcp_tool (45%), file (46%), feishu_tools (24%), computer_use (0%)
64-67. **Plugin System (4)** — model-provider plugins, memory plugins, other plugins, plugin LLM

## Build Gate
- `make clean && make -j$(nproc)` — 0 errors
- `bash test_runner.sh` — 226/0/23
- Visual: compare C output against Python Hermes side-by-side

## Config
- `SLERMES_HOME` or `HERMES_HOME` or `~/.slermes/`
- `slermes init` to create default config
- `slermes doctor` to verify
