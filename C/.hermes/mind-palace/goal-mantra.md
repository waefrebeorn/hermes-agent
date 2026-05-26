# Slermes C — Goal Mantra (v19)

P0: Full 1:1 drop-in replacement for Python Hermes.
**1,889 function-level parity gaps** (battleship-v14). 42% parity at function level.

## Phase Order
0. Display Parity (16) — 7/16 done
1. CLI Args (40) — ✅ ALL DONE
2. Provider Parity (26) — deepen 8 + port 18 missing providers
3. Tool Features (60) — add missing features to existing C tools
4. Missing Tools (37) — port unported Python tool files
5. Gateway (51) — missing platform modules, deepening, infrastructure
6. Agent Modules (72) — unported agent modules + deepen existing
7. Plugins (13) — port remaining plugins
8. Libraries (19) — missing library features
9. Security (15) — hardening
10. Test Coverage (51) — tests for untested modules
11. Config/Infra (10) — config expansion, refactoring

## Next: Phase 2 — Provider Parity (26)
Deepen 8 existing: anthropic (thinking blocks, tool_use stream, caching), openai (strict mode, schema), deepseek (thinking config), gemini (native adapter), bedrock (converse full), azure (identity adapter), openrouter (headers/routing), xai (API features)
Port 18 missing: gmi, copilot, copilot-acp, ollama-cloud, huggingface, openai-codex, opencode-zen, alibaba, minimax, stepfun, zai, xiaomi, nous, novita, nvidia, arcee, azure-foundry, ai-gateway
