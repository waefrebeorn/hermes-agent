# Slermes C — Overnight Map (v4 — 2026-05-25)

## Navigation
- **Battleship:** `battleship-v10.md` — 300 gaps, 18 sectors
- **State:** `state.md` — build metrics, stub stats
- **Goal:** `goal-mantra.md` — loop, phase order, rules
- **Prestige:** `prestige_prompt.md` — priority queue
- **Vault:** `vault/achievements.md` — resolved gaps
- **Upstream:** `vault/hermes-upstream.md` — Python modules not ported

## Quick Start
```bash
cd C/
make -j$(nproc)
./slermes --version
```

## Phase 0a Fixes (do first)
1. I01: Split pipe mode stdin by newlines (cli.c:548-588)
2. I02: Reject unknown flags instead of sending to LLM (main.c)
3. I03: Wire --tui to TUI entry (main.c)
4. I04: Validate --session requires value (main.c)
5. I05: Point logs to ~/.slermes/logs/ (commands.c)
6. I06: Consistent config yaml path (hermes_config.c)
7. I07: Fix DeepSeek reasoning param (provider_deepseek.c)
8. I08: Add zero-state to cron (jobs.c)
