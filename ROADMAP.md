# WuBu Slermes — C Translation Roadmap

**HONEST STATUS: ~8% complete.** 57K LOC C vs 433K+ Python.
Full 1:1 parity target: Q3/Q4 2026 at current velocity.

---

## Current Reality (not dashboard fantasies)

| Subsystem | Completeness | Gap |
|-----------|------------|-----|
| Config keys | **3.8%** (16/424) | 408 keys missing. #1 blocker. |
| Tools | 53 names (30 stubs) | 15 missing: discord(2), feishu(5), MoA, video(2), yuanbao(6). MCP=0%. |
| CLI commands | 72 names, ~45% impl | Most printf stubs. No real /save /load /sessions /stats /history. |
| Providers | 3/29+ | Missing 26: OpenRouter, Bedrock, Azure, DeepSeek, xAI, Gemini-native, Moonshot, Alibaba, etc. |
| Gateway platforms | 19 (shallow) | Count close but features thin. |
| MCP | **0%** | No dynamic tools. Agent can't use MCP. #2 blocker. |
| Plugin system | **0%** | 17 plugin types. Zero. |
| Session DB | grep-based | No SQLite, no FTS5, no metadata, no branch. |
| Delegation | basic subprocess | No concurrency, no orchestrator, no child isolation. |
| Security | basic | No redaction, no blocklist, no allowlist, no audit log. |
| Agent loop | 332/12,000 LOC | No budget, no fallback, no credential pool, no checkpoint. |
| TUI | 926/41,000 LOC | Bare ncurses vs React/Ink with split panes, streaming, themes. |
| Tests | 43/~17,000 | No integration, E2E, or fuzz tests. |
| ACP/LSP | **0%** | No IDE integration. |
| Memory | file-store | No TTL, no dedup, no provider plugins. |
| Cron | basic loop | No SQLite persistence, no retry, no chaining. |
| Skills | basic | No hub, sync, provenance, bundles, curator. |

---

## 200-Phase Roadmap

### Phase Group 1: Config System (P1-P25) — #1 Priority
- P1-P15: Expand hermes_config_t: provider→display→agent→tools→delegation→browser→memory→compression→cron→notification→security→session→plugin→MCP config groups
- P16-P20: Config validation, env var override, profiles, diff/show, watch
- P21-P25: Config import/export, constants module, merge logic, schema generation, migration

### Phase Group 2: CLI Commands (P26-P40)
- P26-P40: Full implementations for /new /clear /undo /save /load /sessions /stats /conv /history /model /config /topic /personality /tools /tools-verify /help /reset /retry /branch /snapshot /compress /approve /deny /yolo /plugins /cron /platform

### Phase Group 3: Tool Infrastructure (P41-P55)
- P41-P42: discord + discord_admin tools
- P43-P44: feishu_doc + feishu_drive tools (5 total)
- P45: mixture_of_agents tool
- P46-P47: video_analyze + video_generate tools
- P48: yuanbao tools (6)
- P49-P55: tool result storage, output limits, backend helpers, timeout, retry, dependency injection, pattern matching

### Phase Group 4: MCP System (P56-P70) — #2 Priority
- P56-P58: MCP client library, stdio transport, tool registration
- P59-P61: MCP tool dispatch, config discovery, server lifecycle
- P62-P65: SSE transport, auth, OAuth manager, namespace
- P66-P70: Tool filtering, resource access, sampling, prompt templates, root filesystem

### Phase Group 5: Provider System (P71-P85)
- P71-P72: Abstract provider interface + plugin system
- P73-P81: OpenAI, Anthropic, Google, OpenRouter, Bedrock, Azure, DeepSeek, xAI, Custom provider ports
- P82-P85: Credential pool, fallback routing, budget tracking, provider metadata

### Phase Group 6: Agent Loop (P86-P100)
- Iteration budget, parallel tool calls, grace call, interrupt handling, history management
- System prompt caching, prefill, tool result classification, reasoning content
- Streaming diagnostics, compression, checkpoints, background review

### Phase Groups 7-15: Everything Else (P101-P200)
- Gateway depth, delegation system, plugin system (17 types), session DB, memory system
- Security (redaction, blocklist, allowlist, audit log), cron scheduler, skills system
- TUI (React/Ink parity), testing (43→17,000), ACP adapter, LSP, CI pipeline, docs site

---

## Build

```bash
# Dependencies
sudo apt install libsqlite3-dev libcurl4-openssl-dev libyaml-dev libssl-dev ncurses-dev

# Build everything
make -C C -j$(nproc)

# Run
./C/hermes "Hello"
```

## Related Docs

| Doc | Location |
|-----|----------|
| 200-phase detailed plan | `C/.hermes/mind-palace/plans/200-phase-roadmap.md` |
| DA audit v3 (honest) | `C/.hermes/mind-palace/plans/devils-advocate-v3.md` |
| Current state | `C/.hermes/mind-palace/state.md` |
| Goal mantra | `C/.hermes/mind-palace/goal-mantra.md` |
| Prestige prompt | `C/.hermes/mind-palace/prestige_prompt.md` |
