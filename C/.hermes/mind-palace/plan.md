# Slermes C — Plan (May 22 DA v5)

## Current State
- Overall: **~57% complete** (DA v5)
- **P1-P25 (Config):** All 25 phases implemented ✅ P15/P19/P22 gap-filled
- Config keys: 206/322 (64%) ← 116 still missing (leaf key expansion, not phase gaps)
- Tools: 74 registered, 54 expected all found (92%)
- CLI: 72/85 commands (85%), all dispatch, most feature-complete
- **Providers: 9/29 (~31%)** ← 6 native added (OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom)
- MCP: core + 6 tools, namespace wired, sampling/roots missing (70%)
- Agent loop: budget, parallel dispatch, interrupt, checkpoints (75%)
- Tests: <1% (15 files, 1.8K LOC vs ~17K) ← CRITICAL gap
- LLM: ✅ DeepSeek v4 Flash via OpenAI-compat, runtime-verified

## Active Phase
**P71-P85: Provider expansion — 20 providers missing (31%, 9/29).**
Config P1-P25 structural phases now complete. Terminal/logging/skills/checkpoints config groups added.
DeepSeek, OpenRouter, xAI, Azure, Bedrock, Custom now native. Next: Nous, Alibaba, StepFun, etc.

### Just Completed (This Session)
- **+40 config keys**: terminal(22), logging(5), skills(5), checkpoints(8)
- **Config struct**: 4 new sub-structs in hermes_config_t
- **YAML parsing, env overrides, validation, diff, export** all wired for new groups
- **Backward compat**: terminal fields sync to old tools_config_t
- Commits: a810e9593 (this DA session)

## Next Phases In Queue
1. **Config gap (116 keys)** — auxiliary(56), tts(17), discord(12), kanban(10), bedrock(8), tool_loop_guardrails(8), curator(7), stt(6), voice(6), etc.
2. **Providers (20 missing)** — Nous, Alibaba, StepFun, Minimax, Novita, ZAI, HuggingFace, Arcee, Ollama Cloud, Nvidia, GMI, KiloCode, Kimi, AI Gateway, Azure Foundry, Xiaomi, Qwen OAuth, Copilot ACP, OpenCode Zen, Codex
3. **Tools (P41-P55 gap)** — 14 missing: feishu doc+drive (5), video analyze+generate (2), yuanbao (6), MoA (1)
4. **Plugins (P126-P140)** — 12+ plugin types: memory providers, context engine, model providers, image/video gen, browser, achievements, observability
5. **TUI (P189-P200)** — 6/12 phases: theme engine, session browser, config editor, image viewer, gateway backend, mobile mode
6. **Security** — file sandbox (directory restriction for file tools)
7. **Tests** — C test harness, port ~17K Python tests

## Milestones
- **M1 (Config gap closed):** 322 keys, profile merge, hot-reload, validation
- **M2 (Provider parity):** 29+ providers, credential pool, fallback, budget
- **M3 (All 200 phases implemented):** Full feature set. DA v5 at >90%.
- **M4 (Tests):** Port critical Python tests, runtime verify all subsystems
- **M5 (Full 1:1 parity):** Python ground truth verified for every subsystem

## Recent Wins (May 21-22)
- Config: +40 keys in 4 groups (terminal, logging, skills, checkpoints)
- Providers: 9/29 native (added OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom)
- Tests: P85 metadata test (52/52), P71 smoke test (131/131), P98 checkpoints (44/44)
- Config validation: all 14 groups validated
- Config hot-reload: SIGHUP-based
- Config merge: full field-level merge for all config groups
- File sandbox: directory restriction for file tools (P168)
- DA v5: full sweep with corrected counts (206/322 config, 9/29 providers)
- LLM: end-to-end working with DeepSeek v4 Flash via OpenAI-compat
- API key paranoia scan: git history clean
