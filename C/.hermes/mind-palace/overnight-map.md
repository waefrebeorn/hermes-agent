# Slermes C — Overnight Map

## Navigation
- **State:** `.hermes/mind-palace/state.md` — build metrics, known gaps
- **Goal:** `.hermes/mind-palace/goal-mantra.md` — loop, rules
- **Prestige:** `.hermes/mind-palace/prestige_prompt.md` — priority queue
- **Battleship:** deprecated — gaps documented in state.md

## Current Branch
`main` — full C codebase merged from slermes branch

## Build
```bash
cd C/
make -j$(nproc)       # 30MB slermes binary
bash test_runner.sh   # 226/0/23
```

## Commands
| Command | Action |
|---------|--------|
| `slermes init` | Create ~/.slermes/config.yaml + .env |
| `slermes doctor` | Diagnostics (config, keys, tools) |
| `slermes completions install` | Shell completion setup |
| `make install` | Install to /usr/local/bin |

## Last Session
- Renamed C translation from hermes → slermes
- Merged full codebase (84 tools, 59 libs, 19 gateways, 10 providers)
- Added: init, doctor, completions install, make install/uninstall
- Triple DA audit: 6 verified missing agent modules, 13 gateway sub-platforms

## Fallback Task
Pick next P1 gap from prestige_prompt.md and implement it.
