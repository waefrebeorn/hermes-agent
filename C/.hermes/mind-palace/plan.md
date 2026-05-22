# Slermes C — Plan (May 21 DA v4)

## Current State
- Overall: **~57% complete** (P76 OpenRouter native done this session)
- **P1-P25 (Config):** All 25 phases implemented ✅ P15/P19/P22 gap-filled
- Config keys: 154/318 (48%) ← 164 still missing (leaf key expansion, not phase gaps)
- Tools: 74 registered, 54 expected all found (92%)
- CLI: 72/85 commands (85%), all dispatch, most feature-complete
- **Providers: 4/29+ (~15%)** ← OpenRouter native added (P76)
- MCP: 70% (core client + 6 tools)
- Plugins: 25% (3/17 types)
- Tests: <1% (12 files, 1.3K LOC vs ~17K) ← CRITICAL gap
- LLM runtime: WORKING (DeepSeek v4 Flash tested)

## Active Phase
**P71-P85: Provider expansion — 26 providers missing (10%, 3/29).** 
Config P1-P25 structural phases now complete. Moving to biggest gap by count.
OpenRouter, Groq, Together, Bedrock, Azure, xAI top priority.
DeepSeek works via OpenAI compat.

### Just Completed (This Session)
- **P15: Config validation** — all 14 groups now validated (type/range/enum)
- **P19: Config hot-reload** — SIGHUP handler reloads config; fallback on error
- **P22: Config merge logic** — full field-level merge for all config groups
- **P168: File sandbox** — wired into tools_init_all(); HOME/tmp/SLERMES_HOME allowed
- **YAML parser gap-fill** — ~16 struct fields now populated from config.yaml
- Commits: aafcc0e44, edf1d764d, 1768f1c97, 6b9cc5846, 010ebbb4a

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
