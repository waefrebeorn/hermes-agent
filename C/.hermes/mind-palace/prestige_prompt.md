# WuBu Hermes C — Prestige Prompt (v13 — 2026-06-07)

## Identity
1:1 C reimplementation of Python hermes-agent. 144 source files (115 .c + 29 .h), 56 library units, 78 tools, 19 gateways, 10 plugins, ~148 CLI commands. ~410 C-specific commits. Synced upstream.

## Current State
| Metric | Value |
|--------|-------|
| Suite | 197/1/0 (50+ test files, ~1,200 assertions) |
| Binary | 9.1MB dynamic ELF |
| Tools registered | 78 |
| Gateway platforms | 19 |
| Plugins .so | 10 |
| CLI commands | ~148 |
| Config keys | ~322 |
| Parity | ~65% (~323/500) |
| Upstream | 0 behind, 400+ ahead |
| Stubs | 0 — ALL resolved |
| C LOC | 75.5K (src) |

## Agent Sector
| Module | Status | Notes |
|--------|--------|-------|
| agent_loop | ✅ Complete | Core conversation loop |
| llm_client | ✅ Complete | Chat completions + streaming |
| provider adapters (9) | ✅ Complete | OpenAI, Anthropic, DeepSeek, xAI, Google, Azure, Bedrock, OpenRouter, Custom |
| tool_guardrails | ✅ Complete | 475L file_safety + tool_call guardrails |
| i18n | ✅ Complete | 258L internationalization |
| subdir_hints | ✅ Complete | 224L subdirectory hints |
| onboarding | ✅ Complete | 193L onboarding flow |
| rate_limit_tracker | ✅ Complete | as libratelimit |
| fallback_routing | ✅ Complete | Provider failover routing |
| credential_pool | ✅ Complete | Credential rotation pool |
| budget_tracker | ✅ Complete | Iteration budget tracking |
| error_classifier | ✅ NEW (Jun 7) | 1134L → C, 25 failover reasons, all pattern lists ported |
| checkpoint | ✅ Complete | Session checkpoint/resume |
| context | ✅ Complete | Context window management |
| system_prompt | ✅ Complete | System prompt builder |
| audit | ✅ Complete | DA audit engine |
| retry_utils | ✅ Complete | Retry with backoff |
| vault | ✅ Complete | Vault documentation engine |
| tool_dispatch_helpers | ✅ Complete | as libtooldispatch |
| rate_guard | ✅ Complete | as librateguard |
| agent_init (1638L) | ❌ | Complex Python SDK deps |
| agent_runtime_helpers (2189L) | ❌ | Runtime helpers |
| auxiliary_client (5289L) | ❌ | Large, Python async |
| insights (930L) | ❌ | Session analytics |
| usage_pricing (877L) | ❌ | Cost estimation |
| error_classifier (1134L) | ✅ | Ported |
| +40 more agent modules | ❌ | Various sizes |

## Priority Queue — Remaining Gaps (177)

### P0 — None (all critical stubs resolved)

### P1 — Largest Sectors
| # | Sector | Done | Remaining | Next Gap |
|---|--------|------|-----------|----------|
| 1 | Agent | 44/115 | **71** | image_routing.py (391L), background_review.py (587L) |
| 2 | Gateway | 22/64 | **42** | Per-platform tests, helpers |
| 3 | Tools | 66/92 | **26** | skill_manager_tool.py (931L), skills_guard.py (1007L) |
| 4 | CLI | 79/95 | **16** | Autocomplete, rich formatting |
| 5 | Plugins | 10/26 | **16** | Provider plugins |

### P2 — Feature Depth
| # | ID | What | Effort |
|---|----|------|--------|
| 1 | image_routing.py | Image routing for native vision support | 391L, medium |
| 2 | background_review.py | Background code review for skills | 587L, medium |
| 3 | skill_manager_tool.py | Skill CRUD + curator | 931L, large |
| 4 | account_usage.py | Provider account usage API | 326L, medium |
| 5 | insights.py | Session analytics from DB | 930L, large |
| 6 | usage_pricing.py | Token cost estimation | 877L, large |

## Key Files
- **State:** `state.md` (binary truth table)
- **Battleship:** `plans/battleship-v2.md`
- **DA v13:** `da-audit-v13.md` (latest full audit)
- **Bugs:** `vault/bug-bounty.md`
