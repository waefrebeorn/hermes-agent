# WuBu Hermes C — Prestige Prompt (v14 — 2026-05-23)

## Identity
1:1 C reimplementation of Python hermes-agent. 144 source files (115 .c + 29 .h), 56 library units, 78 tools, 19 gateways, 10 plugins, ~148 CLI commands. ~412 C-specific commits. Synced upstream.

## Current State
| Metric | Value |
|--------|-------|
| Suite | 197/1/0 (50+ test files, ~1,200 assertions) |
| Binary | 29.8MB dynamic ELF |
| Tools registered | 78 |
| Gateway platforms | 19 |
| Plugins .so | 10 |
| CLI commands | ~148 |
| Config keys | ~322 |
| Parity | ~65% (~324/500) |
| Upstream | 0 behind, 400+ ahead |
| Stubs | 0 — ALL resolved |
| C LOC | ~76K (src) |

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
| error_classifier | ✅ Complete | 1134L → C (Session 52), 25 tests |
| checkpoint | ✅ Complete | Session checkpoint/resume |
| context | ✅ Complete | Context window management |
| system_prompt | ✅ Complete | System prompt builder |
| audit | ✅ Complete | DA audit engine |
| retry_utils | ✅ Complete | Retry with backoff |
| vault | ✅ Complete | Vault documentation engine |
| tool_dispatch_helpers | ✅ Complete | as libtooldispatch |
| rate_guard | ✅ Complete | as librateguard |
| image_routing | ✅ NEW (May 23) | Config-aware native vs text image routing, 34 tests |
| agent_init (1638L) | ❌ | Complex Python SDK deps |
| agent_runtime_helpers (2189L) | ❌ | Runtime helpers |
| auxiliary_client (5289L) | ❌ | Large, Python async |
| insights (930L) | ❌ | Session analytics |
| usage_pricing (877L) | ❌ | Cost estimation |
| background_review (587L) | ❌ | Background code review for skills via curator fork |
| +39 more agent modules | ❌ | Various sizes |

## Priority Queue — Remaining Gaps (~176)

### P0 — None (all critical stubs resolved)

### P1 — Largest Sectors
| # | Sector | Done | Remaining | Next Gap |
|---|--------|------|-----------|----------|
| 1 | Agent | 45/115 | **70** | background_review.py (587L) |
| 2 | Gateway | 22/64 | **42** | Per-platform tests, helpers |
| 3 | Tools | 66/92 | **26** | skill_manager_tool.py (931L), skills_guard.py (1007L) |
| 4 | CLI | 79/95 | **16** | Autocomplete, rich formatting |
| 5 | Plugins | 10/26 | **16** | Provider plugins |

### P2 — Feature Depth
| # | ID | What | Effort |
|---|----|------|--------|
| 1 | background_review.py | Background code review for skill pruning via curator fork | 587L, medium |
| 2 | skill_manager_tool.py | Skill CRUD + curator + hub publishing | 931L, large |
| 3 | account_usage.py | Provider account usage API | 326L, medium |
| 4 | insights.py | Session analytics from DB | 930L, large |
| 5 | usage_pricing.py | Token cost estimation | 877L, large |

## Key Files
- **State:** `state.md` (binary truth table)
- **Goal-Mantra:** `goal-mantra.md` (v14)
- **DA v13:** `da-audit-v13.md` (latest full audit, Jun 1)
- **Bugs:** `vault/bug-bounty.md`
