# Slermes C — State (May 22 v4, Sessions 12+12b: 11 LLM param gaps closed)

## Honest Assessment
- **Session 12:** G330-G333 + G340 — wired temperature, top_p, stop_sequences, max_tokens, service_tier through all 9 providers. Structural fix: added `provider_config_t` embedded in `provider_t`, extended `llm_config_t` with LLM params.
- **Session 12b:** G334-G335 + G337-G338 — wired presence_penalty, frequency_penalty, seed, logprobs/top_logprobs, user through 6 OpenAI-compat providers. Config pipeline: defaults, YAML, env (6 new HERMES_ vars), validation, diff.

## Current Milestones

| Area | Progress | Remaining |
|------|----------|-----------|
| Config keys | **~99%** (322/322) | 6 depth features (profiles, schema, watch, migration) |
| Providers | **90%** (26/29) | 3 ACP (Copilot, OpenCode, Codex) |
| LLM params | **83%** (10/12) | G336 response_format, G339 metadata |
| CLI | **85%** (72/85) | 13 advanced commands |
| Tools | **82%** (74/88) | 14 missing + 7 stubs + 3 shallow |
| Gateway | **95%** (19 platforms) | Depth (Telegram 11x, send methods, msg types) |
| Security | **85%** | 2 phases (vault encryption, session isolation) |
| Tests | **~1,422 assertions** (58 suites) | MCP, integration, benchmarks, CI |
| Agent loop | **75%** | Grace call, prefill, stream diag, background |
| MCP | **70%** | 4 phases (subscriptions, sampling, prompts, roots) |
| Plugin system | **50%** | Provider plugins, memory plugins |
| TUI | **50%** | 6 phases |
| **Overall (430-gap)** | **~57%** | Weighted effort across 430 gaps |

## Key Remaining Gaps (top by impact)

- **7 C stubs (P0):** CDP browser tool stubs (4), computer_use returns unsupported, memory_sqlite falls back to file, memory_plugin falls back to in-mem
- **Telegram 11x depth gap (P1):** C 479 lines vs Python 5,465 — 16 send methods, 10 message types missing
- **LLM params (P1):** 2 remain — G336 (response_format needs JSON schema struct), G339 (metadata needs key-value map)
- **3 shallow tools (P2):** kanban 9 C handlers vs 25 Py functions, browser 18 vs 158 Py, memory 3 vs 22 Py
- **14 Python libs unported (P3):** Jinja2, prompt_toolkit, httpx, rich, pydantic, etc.
- **10 session tracking fields (P2):** per-direction tokens, cost tracking, interrupt propagation
- **5 CLI features (P2):** autocomplete, table output, wizard, batch mode, color depth

## Known Bug (DA find)
- `temperature=0.0` (greedy decode) silently dropped by `> 0.0f` guard in all 9 providers. User's explicit greedy request ignored. Fix: change to `>= 0.0f`.

## 430-gap roadmap
`.hermes/mind-palace/plans/400-gap-mega-roadmap.md`
