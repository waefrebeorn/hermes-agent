# Hermes C — Entry (v11 — 2026-05-23)

## Build & Run
```bash
cd /home/wubu/hermes-agent-dev/C/
make -j$(nproc)                   # Build (0 errors, ~40 warnings)
bash test_runner.sh                # 154/0/0
./hermes --version                 # v0.14.1+
./hermes --help                    # Usage
./hermes "hello"                   # One-shot
./hermes                          # Interactive (/help)
./hermes --gateway --platform telegram  # Gateway mode
make tui && ./hermes-tui          # ncurses TUI (partial)
```

## Quick Stats
```
Suite:   154/0/0  (117 tests, ~573 assertions)
Binary:  9.1MB   (dynamically linked)
Source:  115 .c + 29 .h = 144 files, 75.5K LOC
Tools:   68 registered
Gateway: 19 platforms
Plugins: 10 .so
Parity:  ~32%    (161/500 gaps — battleship v3)
```

## Key Paths
- **Source:** `/home/wubu/hermes-agent-dev/C/`
- **Config:** `~/.slermes/config.yaml` (~322 keys) + `~/.slermes/.env`
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `plans/battleship-v2.md` (500 gaps)
- **Vault:** `.hermes/vault/` (essays, bug bounty, credits)
- **DA v11:** `da-audit-v11-500-goals.md`

## Architecture
```
CLI/Gateway → Agent Loop → LLM Client → 9 Providers (OpenAI-compat + native)
                 ↓
          68 Tools Registry
                 ↓
          10 Plugin .so (kanban, honcho, spotify, ...)
                 ↓
          30 Library Units (json, yaml, http, crypto, ...)
```

## 4 Verified Stubs
| Feature | File | Why Broken |
|---------|------|-----------|
| computer_use | `src/tools/computer_use.c` | Needs macOS cua-driver |
| browser CDP | `src/tools/browser.c` (5/11 tools) | Needs Chrome CDP |
| image_gen | `src/plugins/plugin_image_gen.c` | Fake URLs, no backend |
| TUI sessions | `src/cli/tui_fullscreen.c` | Hardcoded "current" |

## Upstream
0 behind (183 Python commits merged), 400 ahead (C-specific commits).
