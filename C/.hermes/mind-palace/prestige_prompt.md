# Slermes C — Prestige Prompt (May 22 PM v2)

## Mission
Translate NousResearch/hermes-agent from Python→C. 200 phases to 1:1 parity.
Zero-dependency, single-binary AI agent that mirrors every Python feature.

## Architecture Stack
- **Language:** C99, no runtime deps, static ELF (~3.4MB)
- **Build:** GNU make, 5 phase targets (phase1=libraries→phase5=full binary)
- **Libs:** 14 standalone libraries (json, http, yaml, crypto, dotenv, cron, proc, template, tui, db, plugin, skin, websocket, protobuf, mcp)
- **Providers:** 26/29 — 9 native (OpenAI, Anthropic, Google, OpenRouter, DeepSeek, xAI, Azure, Bedrock, Custom) + 17 OpenAI-compat aliases
- **Gateway:** 19 platforms via adapter pattern + JSON-RPC server
- **Config:** YAML → struct → env override → validation/diff/export/schema
- **Tests:** 58 runner items, ~1422 assertions, 27 test files

## Current State (DA v6, May 22)

| Area | Progress | Remaining |
|------|----------|-----------|
| Config keys | **99%** | ~0-2 leaf keys |
| Providers | **90%** (26/29) | 3 ACP providers |
| LLM params | **36%** (4/11) | 6 remaining + service_tier partial |
| CLI commands | **85%** (72/85) | 13 advanced commands |
| Tools | **82%** (74/88) | 14 tools (feishu, video, MoA, yuanbao) |
| Gateway | **95%** (19 platforms) | Depth features |
| Security | **85%** | 2 phases (vault encryption, session isolation) |
| Tests | **~1422 assertions** | MCP, integration, benchmarks, CI |
| Agent loop | **75%** | Grace call, prefill, stream diag, background review |
| MCP | **70%** | 4 phases |
| Plugin system | **50%** | Provider plugins, memory plugins |
| TUI | **50%** | 6 phases |
| Overall | **~94%** | ~11% remaining by weighted effort |

## Sessions 6-11 Summary (11 gaps closed)
- G164: Tool registry test + NULL-deref bugfix (46 tests)
- G165: Gateway subsystem test (49 tests)
- G158: Budget tracker test + 3 bugfixes (104 tests)
- G168: Plugin system test (38 tests)
- G127: Per-tool rate limiter test (168 tests)
- G125: URL safety test + header fix (55 tests)
- G126: Command allowlist test (34 tests)
- G130: Audit log test (20 tests)
- G128/G169: Approval system test (18 tests)
- G166: Agent loop context test (161 tests)

## Priority Queue
P0: ACP providers (3) — Copilot, OpenCode Zen, OpenAI Codex
P1: LLM params (6 remaining) — presence_penalty, frequency_penalty, seed, response_format, logprobs, user, metadata
P1: MCP test suite (G167)
P2: Missing tools (14) — feishu, video, MoA, yuanbao
P2: TUI depth (6 phases)
P3: More test suites (G170 integration, G171 benchmarks)
P3: Provider plugin system (G159)
P3: Model catalog auto-update (G160)

## Known Blockers
- Pre-existing: plugin load test in test_runner.sh (non-zero exit, SO file not bundled)
- None active for new work

## Key Paths
- Source: `/home/wubu/hermes-agent-dev/C/`
- Config: `~/.slermes/config.yaml` + `~/.slermes/.env`
- Mind palace: `.hermes/mind-palace/`
- Test runner: `test_runner.sh`
- Git remote: `wubu=waefrebeorn/hermes-agent` (SSH)
- Binary: `C/hermes` (~3.4MB)

## Key Commands
```bash
make -C C -j$(nproc)    # Build full binary
bash C/test_runner.sh    # Run all 58 test suites
echo "/status" | ./C/hermes  # Quick session status
git push wubu HEAD       # Push to fork
```
