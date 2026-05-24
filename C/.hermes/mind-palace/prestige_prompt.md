# WuBu Hermes C — Prestige Prompt (Session ~57 — 2026-05-23)

## Identity
1:1 C reimplementation of Python hermes-agent. 44 agent C files, 57 library units, 82 tools, 19 gateways, 10 plugins, ~148 CLI commands, 173 test files. Synced upstream.

## Current State
| Metric | Value |
|--------|-------|
| Suite | 211/0/0 (173 test files, ~1500 assertions) |
| Binary | 29MB dynamic ELF |
| Tools registered | 82 |
| Gateway platforms | 19 |
| Plugins .so | 10 |
| CLI commands | ~148 |
| Config keys | ~322 |
| Parity | ~65% (~324/500) |
| Stubs | 0 — ALL resolved |
| C LOC | ~76K (src) |

## Agent Sector
| Module | Status | Notes |
|--------|--------|-------|
| agent_loop | ✅ Complete | Core conversation loop |
| llm_client | ✅ Complete | Chat completions + streaming |
| provider adapters (9) | ✅ Complete | OpenAI, Anthropic, DeepSeek, xAI, Google, Azure, Bedrock, OpenRouter, Custom |
| tool_guardrails | ✅ Complete | 475L port |
| i18n | ✅ Complete | 258L port, 22 tests |
| subdir_hints | ✅ Complete | 224L port |
| onboarding | ✅ Complete | 193L port, 25 tests |
| rate_limit_tracker | ✅ Complete | as libratelimit |
| fallback_routing | ✅ Complete | Provider failover routing |
| credential_pool | ✅ Complete | Credential rotation pool |
| budget_tracker | ✅ Complete | Iteration budget tracking |
| error_classifier | ✅ Complete | 1134L → C, 25 tests |
| checkpoint | ✅ Complete | Session checkpoint/resume |
| context | ✅ Complete | Context window management |
| system_prompt | ✅ Complete | System prompt builder |
| audit | ✅ Complete | DA audit engine |
| retry_utils | ✅ Complete | Retry with backoff |
| vault | ✅ Complete | Vault documentation engine |
| tool_dispatch_helpers | ✅ Complete | as libtooldispatch |
| rate_guard | ✅ Complete | as librateguard |
| image_routing | ✅ Complete | Config-aware image routing, 34 tests |
| skill_utils | ✅ Complete | 566L port as libskillutils |
| lmstudio_reasoning | ✅ Complete | 48L port |
| manual_compression_feedback | ✅ Complete | 49L port |
| prompt_caching | ✅ Complete | 79L port |
| gemini_schema | ✅ Complete | 99L port, 32 tests |
| moonshot_schema | ✅ Complete | 262L port, 17 tests |
| skill_bundles | ✅ Complete | 215L, 18 tests |
| TUI visual parity | ✅ Complete | Tool feed, kawaii, inline diff |
| agent_init (1638L) | ❌ | Complex Python SDK deps |
| agent_runtime_helpers (2189L) | ❌ | Runtime helpers |
| auxiliary_client (5289L) | ❌ | Large, Python async |
| insights (930L) | ❌ | Session analytics |
| usage_pricing (877L) | ❌ | Cost estimation |
| +35 more agent modules | ❌ | Various sizes |

## Priority Queue — Remaining Gaps (~176)

### P1 — Largest Sectors
| # | Sector | Done | Remaining | Next Gap |
|---|--------|------|-----------|----------|
| 1 | Agent | 54/115 | **61** | account_usage.py (326L), image_gen_registry.py |
| 2 | Gateway | 22/64 | **42** | Per-platform tests, helpers |
| 3 | Tools | 66/92 | **26** | skill_manager_tool.py (931L) |
| 4 | CLI | 79/95 | **16** | Autocomplete, rich formatting |
| 5 | Plugins | 10/26 | **16** | Provider plugins |

### P2 — Feature Depth
| # | ID | What | Effort |
|---|----|------|--------|
| 1 | image_gen_registry.py (145L) | Image provider registry | small |
| 2 | video_gen_registry.py (117L) | Video provider registry | small |
| 3 | account_usage.py (326L) | Provider account usage API | medium |
| 4 | insights.py (930L) | Session analytics from DB | large |
| 5 | usage_pricing.py (877L) | Token cost estimation | large |

## Key Files
- **State:** `state.md` (binary truth table)
- **Goal-Mantra:** `goal-mantra.md`
- **DA v14:** `da-audit-v14.md`
- **Overnight:** `overnight-map.md`
- **Bugs:** `vault/bug-bounty.md`
