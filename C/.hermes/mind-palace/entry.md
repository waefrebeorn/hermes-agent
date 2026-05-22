# Slermes C — Entry (May 22 DA v5)

## Build & Run
```bash
cd /home/wubu/hermes-agent-dev/C/
make clean && make -j$(nproc)     # Build (0 errors)
bash test_runner.sh               # 21 tests
./hermes "Say hi"                 # One-shot
./hermes                          # Interactive (/help)
./hermes --gateway --platform telegram  # Gateway
make tui && ./hermes-tui          # ncurses TUI
```

## Shortcuts
```bash
bash ~/hermes-agent-dev/scripts/slermes-build.sh   # Build + test + parity
slermes "Say hi"                                    # Alias (same binary)
```

## Key Paths
- Source: /home/wubu/hermes-agent-dev/C/
- Binary: ./hermes (alias: slermes)
- Config: ~/.slermes/ (SLERMES_HOME)
- Mind palace: .hermes/mind-palace/
- Python ref: /home/wubu/.hermes/hermes-agent/
- ROADMAP: /home/wubu/hermes-agent-dev/ROADMAP.md

## Architecture
```
C/ (hermes binary, aliased as slermes)
├── lib/      → 12 standalone libraries
├── src/
│   ├── agent/    → LLM client, provider interface, agent loop, context
│   ├── cli/      → 16 commands (registry), config, display
│   ├── tools/    → 27 tools (core+browser+approval+provider framework)
│   ├── gateway/  → 7 platforms (Telegram/Discord/Slack/Matrix/Mattermost/Webhook/WhatsApp)
│   └── cron/     → scheduler
├── include/  → hermes.h + sub-headers
└── tests/    → 21 test sources
```
