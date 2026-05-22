# Slermes C — Prestige Prompt (May 22 PM v3)

## Mission
Translate NousResearch/hermes-agent from Python to C. Zero-dependency, single-binary AI agent that mirrors every Python feature across ~200 phases of implementation.

## Architecture Stack
- **Language:** C99, no runtime deps, static ELF (~3.4MB)
- **Build:** GNU make, 5 phase targets (phase1=libraries→phase5=full binary)
- **Libs:** 14 standalone libraries (json, http, yaml, crypto, dotenv, cron, proc, template, tui, db, plugin, skin, websocket, protobuf, mcp)
- **Providers:** 26/29 — 9 native (OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom) + 17 OpenAI-compat aliases
- **Gateway:** 19 platforms via adapter pattern + JSON-RPC server
- **Config:** YAML → struct → env override → validation/diff/export/schema (322 keys)
- **Tests:** 58 runner items, ~1422 assertions, 27 test files

## Current State (DA v6, May 22 v3)

| Area | Progress | Remaining |
|------|----------|-----------|
| Config keys | **99%** | ~0-2 leaf keys |
| Providers | **90%** (26/29) | 3 ACP providers |
| LLM params | **83%** (10/12) | G336 response_format, G339 metadata |
| CLI commands | **85%** (72/85) | 13 advanced commands |
| Tools | **82%** (74/88) | 14 tools (feishu, video, MoA, yuanbao) + 7 stubs + 3 shallow |
| Gateway | **95%** (19 platforms) | Depth features, Telegram 11x |
| Security | **85%** | 2 phases (vault encryption, session isolation) |
| Tests | **~1422 assertions** | MCP, integration, benchmarks, CI |
| Agent loop | **75%** | Grace call, prefill, stream diag, background review |
| MCP | **70%** | 4 phases |
| Plugin system | **50%** | Provider plugins, memory plugins |
| TUI | **50%** | 6 phases |
| Overall | **~57%** | 430-gap weighted effort |

## Sessions 12+12b Summary (11 LLM param gaps closed)
**Session 12:** G330-G333, G340 — wired temperature, top_p, stop_sequences, max_tokens, service_tier through all 9 providers. Structural: provider_config_t embedded in provider_t, llm_config_t extended. Config pipeline: defaults, YAML parse, env override, validation, diff. 20 files, +413/-281.

**Session 12b:** G334-G335, G337-G338 — wired presence_penalty, frequency_penalty, seed, logprobs/top_logprobs, user through 6 OpenAI-compat providers. 6 new HERMES_ env vars. 12 files, +176. 58/59 tests pass.

## Priority Queue
P0: Fix stubs (7) — CDP browser(4), computer_use, memory_sqlite, memory_plugin, vision_analyze LLM
P0: ACP providers (3) — Copilot, OpenCode Zen, OpenAI Codex
P1: Telegram depth — 16 send methods, 10 message types (11x gap vs Python)
P1: LLM params (2 remaining) — G336 response_format, G339 metadata
P2: Tool depth — kanban(9→25), browser(18→158), memory(3→22)
P2: Session tracking fields (10)
P2: CLI features — autocomplete, table output, wizard, batch mode
P3: Test porting — MCP suite, integrations, benchmarks
P3: 14 Python libs (Jinja2, prompt_toolkit, httpx, rich, pydantic, etc.)

## Known Bugs (DA finds)
- `temperature=0.0` (greedy decode) silently dropped by `> 0.0f` guard in all 9 providers. Fix: change to `>= 0.0f`.

## Key Paths
- Source: `/home/wubu/hermes-agent-dev/C/`
- Config: `~/.slermes/config.yaml` + `~/.slermes/.env`
- Mind palace: `.hermes/mind-palace/`
- Test runner: `test_runner.sh`
- Git remote: `wubu=waefrebeorn/hermes-agent` (SSH)
- Binary: `C/hermes` (~3.4MB)
- 430-gap roadmap: `.hermes/mind-palace/plans/400-gap-mega-roadmap.md`

## Key Commands
```bash
make -C . -j$(nproc)    # Build full binary
bash test_runner.sh       # Run all 58 test suites
echo "/status" | ./hermes  # Quick session status
git push wubu HEAD        # Push to fork
```
