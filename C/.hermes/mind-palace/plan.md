# Slermes C — Plan (May 21 DA v4)

## Current State
- Overall: **~45% complete** (up from 8% at DA v3)
- Config keys: 154/318 (48%) ← 164 still missing
- Tools: 74 registered, 54 expected all found (92%)
- CLI: 72/85 commands (85%), all dispatch, most feature-complete
- Providers: 3/29+ (10%) ← BIGGEST gap by count
- MCP: 70% (core client + 6 tools)
- Plugins: 25% (3/17 types)
- Tests: <1% (12 files, 1.3K LOC vs ~17K) ← CRITICAL gap
- LLM runtime: WORKING (DeepSeek v4 Flash tested)

## Active Phase
**P1-P25 gap: Config expansion.** Still 164 keys missing from C struct vs Python DEFAULT_CONFIG.
Missing categories: credential_pool_contexts, fallback_providers, toolset arrays, web.* (search_backend, extract_backend), approval subkeys, logging.*, kanban.*, stt/tts/vision/auxiliary sub-dicts, gateway timeouts, guardrail.*, orchestator.*, goals.*, checkpoints.*, backup.*, personality profiles.

## Next Phases In Queue
1. **Config gap (164 keys)** — credential_pool, fallback_providers, toolset arrays, web.*, etc.
2. **Providers (P71-P85 gap)** — 26 missing. OpenRouter, Groq, Together, Bedrock, Azure, xAI top priority. DeepSeek works via OpenAI compat.
3. **Tools (P41-P55 gap)** — 14 missing: feishu doc+drive (5), video analyze+generate (2), yuanbao (6), MoA (1)
4. **Plugins (P126-P140)** — 12+ plugin types: memory providers, context engine, model providers, image/video gen, browser, achievements, observability
5. **TUI (P189-P200)** — 6/12 phases: theme engine, session browser, config editor, image viewer, gateway backend, mobile mode
6. **Security** — file sandbox (directory restriction for file tools)
7. **Tests** — C test harness, port ~17K Python tests

## Milestones
- **M1 (Config gap closed):** 318 keys, profile merge, hot-reload, validation
- **M2 (Provider parity):** 29+ providers, credential pool, fallback, budget
- **M3 (All 200 phases implemented):** Full feature set. DA v5 at >90%.
- **M4 (Tests):** Port critical Python tests, runtime verify all subsystems
- **M5 (Full 1:1 parity):** Python ground truth verified for every subsystem

Estimated: Q4 2026 at current velocity.

## Recent Wins (May 21)
- Config: v1 nested YAML support, .env for model/provider/base_url, env var overrides
- Tool schemas: all include "type":"object" — DeepSeek no longer rejects
- --help: proper handler (was crashing into LLM call)
- LLM: end-to-end working with DeepSeek v4 Flash
- API key paranoia scan: git history clean
- DA audit: v4 written with all 200 phases assessed
- READMEs: root + C both updated to 45%
