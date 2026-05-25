# Hermes C Translation — Battleship Roadmap v6

**DA Audit (2026-06-01) — Real gap survey, alias-corrected. ~25-30 structural gaps.**
**Not ~394. Most Python modules are ported under different C filenames.**

**Verified State:**
- Suite: 232/0/0 | CLI: 88 cmds | Tools: 70 registered
- Gateway: 19 platforms | Providers: 10 C modules | Libs: 58 units
- Agent .c files: 50 | Test .c files: 195 | Source .c files: 153
- Binary: 29MB, 0 warnings

---

## Sector P: Unported Agent Modules (5 gaps)

| ID | Gap | Priority | LOC | Notes |
|----|-----|----------|-----|-------|
| P01 | insights.py — session analytics | **P1** | 931 | Per-model/platform breakdown, --days filter |
| P02 | stream_diag.py — streaming diagnostics | **P1** | 281 | Ttfb, upstream headers (cf-ray, x-openrouter-provider), retry logging, exception chains |
| P03 | memory_provider.py — plugin interface | P2 | 280 | Memory backend plugin abstraction |
| P04 | context_compressor.py depth | P1 | 1749 | Scaled summary budget, token-budget tail (partly in agent_loop.c) |
| P05 | conversation_compression.py depth | P1 | 604 | Related to context compressor |

## Sector T: Test Coverage Gaps (8 gaps)

| ID | Gap | Priority | LOC | Notes |
|----|-----|----------|-----|-------|
| T01 | test_provider_xai.c | P1 | 367 | xAI/Grok provider — parse/stream untested |
| T02 | test_provider_azure.c | P1 | 295 | Azure OpenAI — deploy URL pattern |
| T03 | test_provider_bedrock.c | P1 | 625 | AWS Bedrock — Converse API |
| T04 | test_provider_openrouter.c | P1 | 344 | Model routing, provider preferences |
| T05 | test_provider_custom.c | P1 | 288 | Custom provider impl |
| T06 | test_agent_loop.c | P2 | 1502 | Agent loop state machine |
| T07 | test_llm_client.c | P2 | 1085 | LLM client with HTTP mocking |
| T08 | Gateway integration tests | P2 | — | 19 platforms, 0 integration tests |

## Sector E: Gateway Sub-modules (11 gaps — all P2)

| ID | Gap | Notes |
|----|-----|-------|
| E01 | api_server.py | REST API gateway (already ported as api_server.c) |
| E02 | feishu_comment.py + rules | Comment formatting |
| E03 | signal_rate_limit.py | Rate limiting |
| E04 | telegram_network.py | Network abstraction |
| E05 | wecom_callback.py | Callback handling |
| E06 | wecom_crypto.py | Crypto support |
| E07 | yuanbao_media.py | Media handling |
| E08 | yuanbao_proto.py | Protobuf |
| E09 | yuanbao_sticker.py | Stickers |
| E10 | base.py adapter helpers | Shared platform base |
| E11 | helpers.py | Platform helpers |

## Sector C: CLI Enhancement (6 gaps — P2)

| ID | Gap | Notes |
|----|-----|-------|
| C01 | auth CLI — OAuth flows | P0 in Python, complex |
| C02 | gateway CLI — start/stop/status | Gateway management |
| C03 | plugins CLI — plugin management | Install/remove/list |
| C04 | secrets CLI — manage API keys | Key management |
| C05 | setup wizard — first-run config | Interactive setup |
| C06 | shell completion — bash/zsh | Completion scripts |

## Sector J: Plugins (5 gaps — P2)

| ID | Gap | Notes |
|----|-----|-------|
| J01 | Plugin .so build system | 10 targets, 0 .so built |
| J02 | Kanban plugin | Multi-agent board |
| J03 | Memory plugins | honcho, mem0, supermemory |
| J04 | Context engine plugins | Plugin adapters |
| J05 | Plugin sandbox | Seccomp isolation |

## Sector L: Config Keys (~15 gaps — P1)

| ID | Config Key | Notes |
|----|------------|-------|
| L01 | agent.tool_use_guard_level | Guardrail config depth |
| L02 | agent.image_input_mode | Image mode config |
| L03 | agent.reasoning_effort per-provider | Per-provider reasoning |
| L04 | agent.quiet_mode integration | CLI quiet mode |
| L05 | tools.enabled/disabled toolsets depth | Toolset filtering |
| L06 | agent.budget (token/cost) depth | Budget config |
| L07 | agent.session_db config | Session DB settings |

## Sector R: Provider API Quirks (~8 gaps — P1)

| ID | Gap | Notes |
|----|-----|-------|
| R01 | Anthropic: streaming content extraction | Verified working on May 29 |
| R02 | Anthropic: tool_use content blocks | Content block format |
| R03 | OpenAI: response_format strict mode | Structured output |
| R04 | Google: context caching | Gemini caching |
| R05 | DeepSeek: FIM integration | Fill-in-the-Middle |
| R06 | DeepSeek: reasoning_effort | Reasoning tokens |
| R07 | xAI: reasoning_effort | Grok reasoning |
| R08 | Text/web search multi-provider | Multi-engine support |

---

## Summary

| Sector | Gaps | Priority |
|--------|------|----------|
| P: Unported Agent Modules | 5 | **P1** |
| T: Test Coverage | 8 | P1-P2 |
| E: Gateway Sub-modules | 11 | P2 |
| C: CLI Enhancement | 6 | P2 |
| J: Plugins | 5 | P2 |
| L: Config Keys | 7 | P1 |
| R: Provider Quirks | 8 | P1 |
| **Total real gaps** | **~50** (P1: ~20) | |

Prior DA count of ~394 was inflated by ~365 due to filename-alias mismatch.

Generated 2026-06-01 from Triple DA audit: fresh code survey + alias correction + test coverage scan.
