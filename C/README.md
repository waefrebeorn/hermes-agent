# Slermes C — C Translation of Hermes Agent

**Status: ~8% complete.** 57K LOC vs Python's 433K+.

## What's Here

`C/` contains a full C reimplementation of NousResearch/hermes-agent. Single binary, zero Python deps.

| Metric | Current | Python target | Gap |
|--------|---------|---------------|-----|
| LOC | ~57,000 | ~433,000+ | 87% missing |
| Tools | 53 (30 stubs) | 64+dynamic(MCP) | 15 static + MCP missing |
| CLI commands | 72 (most stubs) | 69+ | Most need full impl |
| Config keys | 16 | 424+ | 96% missing |
| Providers | 3 | 29+ | 26 missing |
| Gateway platforms | 19 | 19+ | Feature depth missing |
| MCP | 0 | full | 100% missing |
| Plugins | 0 | 17 types | 100% missing |
| Tests | 43 | ~17,000 | 99.7% missing |

## Build & Run

```bash
make -j$(nproc)
./hermes "Hello world"

# Or from repo root:
slermes "Hello world"
```

## Key Files

| Path | Content |
|------|---------|
| `src/tools/` | 53 tool implementations |
| `src/cli/commands.c` | 72 CLI commands |
| `src/gateway/platforms/` | 19 platform adapters |
| `include/hermes.h` | Master header (config_t, agent_state_t, tool_t) |
| `src/agent/agent_loop.c` | Core agent loop (332 LOC) |
| `src/agent/llm_client.c` | LLM API client |
| `.hermes/mind-palace/` | Prestige system + 200-phase roadmap + DA audits |

## Full Roadmap

`../ROADMAP.md` at repo root for the complete 200-phase plan.
