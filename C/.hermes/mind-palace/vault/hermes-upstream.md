# Hermes Upstream — C Translation Gap Analysis

## Python vs C Size Comparison

| Metric | Python | C | C/Python Ratio |
|--------|--------|---|----------------|
| App LOC | 451,196 | 83,251 | 18.5% |
| Test LOC | 462,886 | ~25,000 | 5.4% |
| Tool files | 78 | 72 unique tools | 92% |
| Agent modules | 82 | ~40 source files | 49% |
| Model providers | 27 plugins | 9 providers | 33% |
| Gateway files | 40+ files | 19 platforms | 48% |
| Plugins | 17+ dirs | 10 .c files | 59% |
| Test files | 1,220 | 213 | 17% |
| Libraries | N/A (pip) | 59 units | N/A |

## Python Modules Requiring Porting

### Core Agent (46 modules, ~20K LOC)
agent_init (~400), agent_runtime_helpers (~300), auxiliary_client (~500),
background_review (~400), chat_completion_helpers (~400), codex_runtime (~800),
context_compressor (~600), context_engine (~500), conversation_compression (~400),
credential_sources (~300), curator (~1000), curator_backup (~300),
error_classifier (~500), gemini_schema (~400), google_code_assist (~300),
google_oauth (~400), memory_manager (~500), memory_provider (~400),
onboarding (~500), plugin_llm (~400), portal_tags (~300), process_bootstrap (~400),
retry_utils (~200), title_generator (~300), tool_dispatch_helpers (~400),
tool_executor (~600), tool_result_classification (~300),
manual_compression_feedback (~200), moonshot_schema (~300), models_dev (~400),
tts_provider (~300), tts_registry (~200), transcription_provider (~300),
transcription_registry (~200), video_gen_provider (~300), video_gen_registry (~200),
web_search_provider (~300), web_search_registry (~200)

### Gateway Platforms (14 missing files, ~7K LOC)
feishu_comment (1382), feishu_comment_rules (429), signal_rate_limit (369),
telegram_network (259), wecom_callback (403), wecom_crypto (142),
yuanbao_media (645), yuanbao_proto (1209), yuanbao_sticker (558),
chunked_upload (200), keyboards (300), onboard (150), adapter (200),
_http_client_limits (150)

### Tools (15 missing ports, ~3.5K LOC)
microsoft_graph_auth (300), microsoft_graph_client (400), skills_guard (300),
skills_ast_audit (250), skill_provenance (200), yuanbao_tools (300),
slack_blocks (400), website_policy (250), tool_output_limits (200),
tool_result_storage (200), threat_patterns (300), binary_extensions (100),
debug_helpers (150), slash_confirm (100), neutts_synth (300)

### Model Providers (18 missing, plugin-based)
alibaba, arcee, ai-gateway, azure-foundry, copilot-acp, gmi, huggingface,
kilocode, kimi-coding, minimax, nous, novita, nvidia, ollama-cloud,
openai-codex, opencode-zen, qwen-oauth, stepfun, xiaomi, zai

## Roadmap Extension

### Current Milestone: 316 verified gaps (battleship-v13)

### Phase Order & Estimated Timeline (in sessions, not time)
1. **Phase 0b (16 gaps)** — Display parity: inline diffs, multi-line, rich errors, TUI, voice, /recap, tips, output helpers
2. **Phase 0c (40 gaps)** — CLI args: wire (void)args for 40 commands
3. **Phase 1 (10 gaps)** — Form-not-function: fix entry points that do nothing
4. **Phase 2 (8 gaps)** — Missing subcommands: init, doctor, version, completions, gateway, tui, interactive, logs
5. **Phase 3 (28 gaps)** — Tool depth: add Python features to existing C tools
6. **Phase 4 (20 gaps)** — Missing tool ports: port remaining Python tools
7. **Phase 5 (49 gaps)** — Gateway depth: port missing platforms + deepen thin ones
8. **Phase 6 (30 gaps)** — Provider parity: port 18+ missing model providers + deepen 9 existing
9. **Phase 7 (61 gaps)** — Agent modules: port 46 agent modules + 15 infrastructure modules
10. **Phase 8 (8 gaps)** — Security: URL safety, sandbox, encryption, approval, skills guard
11. **Phase 9 (20 gaps)** — Test coverage: tests for untested major modules
12. **Phase 10 (10 gaps)** — Refactoring: thread safety, memory pools, error types, config expansion
13. **Phase 11 (16 gaps)** — Library depth: schema validation, HTTP/2, full regex, etc.
14. **Phase 12 (20 gaps)** — Ecosystem: ACP, cron, gateways full, TUI, voice, plugins, etc.

### Key Dependencies
- Phase 3-7 depend on Phase 0b-2 completing first (core infrastructure stable)
- Phase 9 (test coverage) can run in parallel with any phase
- Phase 12 (ecosystem) is final — depends on all prior phases completing
- Phase 10-11 (refactoring) are ongoing — can be done incrementally

### Verification Milestones
- **~150 gaps closed**: C parity reaches 50% of Python app LOC
- **~200 gaps closed**: Full CLI parity, all tools ported
- **~250 gaps closed**: All providers ported, gateway platforms at parity
- **~316 gaps closed**: Full parity with Python Hermes
