# Slermes C — Overnight Map (v3 — 2026-05-25)

## Navigation
- **State:** `state.md` — visual parity table, gap overview
- **Goal:** `goal-mantra.md` — visual parity now mandatory P0
- **Prestige:** `prestige_prompt.md` — phased priority queue (67 gaps)
- **Battleship:** `battleship-v9.md` — 9 sectors, 67 gaps, phase order
- **DA Audit:** `da-audit-full-parity.md` — comprehensive 10-section audit

## Critical Insight
**Phase 0 (Display Parity) must come FIRST.** Users experience parity through the CLI visuals — banner, spinner, colors, help output, status bar. If these don't match Python Hermes, the product looks broken regardless of backend parity.

C has bare printf. Python has Rich panels, KawaiiSpinner (｡◕‿◕｡), skin engine (30+ hex colors), status bar, prompt_toolkit, and tool emojis.

## Phase Order
1. **Phase 0 (P0)** — 12 display gaps: Skin engine → Spinner → Banner → Status bar → Tool feed → Response box → Help → 256-color → Prompt → Markdown → Faces → Emoji
2. **Phase 1 (P1)** — 4 agent modules: error_classifier, chat_completion_helpers, tool_executor, process_registry
3. **Phase 2 (P2)** — 33 port gaps: 14 tools, 7 adapters, 6 gateway modules, 6 agent modules
4. **Phase 3 (P3)** — 18 depth gaps: 7 small modules, 7 tool depth, 4 plugin system

## Current Branch
`main` — full C codebase, renamed to slermes

## Build
```bash
cd C/
make -j$(nproc)       # 30MB slermes binary
bash test_runner.sh   # 226/0/23
```

## First Action
Read `hermes_cli/skin_engine.py` (926 LOC). Port to C as `src/deps/skin_engine.c` with YAML config loading, 30+ color struct, spinner/default skin. Wire into `cli.c`.

## Fallback
After skin engine, port KawaiiSpinner (`agent/display.py`) to C as animated face renderer. Then use both to build the Rich banner.
