# Hermes C — Entry (v9 — 2026-06-01)

## Build & Run
```bash
cd /home/wubu/hermes-agent-dev/C/
make -j$(nproc)                   # Build full binary (0 errors expected)
bash test_runner.sh                # 154 tests, all passed
./hermes --version                 # "WuBu Hermes v0.14.1"
./hermes "Say hi"                  # One-shot mode
./hermes                          # Interactive mode (/help for commands)
./hermes --gateway --platform telegram  # Gateway mode
make tui && ./hermes-tui          # ncurses TUI (partial)
```

## Shortcuts
```bash
export SLERMES_HOME=~/.slermes
# alias is 'slermes' — same binary, different config path
```

## Key Paths
- **Source:** `/home/wubu/hermes-agent-dev/C/`
- **Binary:** `./hermes` (~9.2M dynamically linked)
- **Config:** `~/.slermes/config.yaml` (~322 YAML keys) + `~/.slermes/.env`
- **Mind palace:** `.hermes/mind-palace/`
- **Python reference:** `/home/wubu/.hermes/hermes-agent/`
- **Vault (docs/essays):** `.hermes/vault/`
- **300-gap roadmap:** `.hermes/mind-palace/plans/300-gap-roadmap-v1.md`

## Architecture
```
C/ (hermes binary)
├── lib/          → 30 .a archives (json, yaml, http, crypto, db, mcp, cron, ...)
├── src/
│   ├── agent/    → LLM client, 9 provider adapters, agent loop, context, budget
│   ├── cli/      → ~148 command handlers, config, display, skin engine, TUI
│   ├── tools/    → 28 registered tools (terminal, file, web, skills, memory, ...)
│   ├── gateway/  → 19 platform adapters (Telegram, Discord, Slack, ...)
│   ├── cron/     → Scheduler, job store, locking, chaining, templates
│   ├── plugins/  → 10 .so plugins (honcho, kanban, spotify, ...)
│   ├── acp/      → ACP server (partial)
│   └── main.c    → Entry point
├── include/      → hermes.h + sub-headers
└── tests/        → 116 test files (154 test functions, all passing)

Suite: 154 passed, 0 failed, 0 skipped
Upstream: 183 commits behind Python hermes-agent
