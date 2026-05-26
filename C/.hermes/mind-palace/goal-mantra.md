# Slermes C — Goal Mantra (v11)

P0: Full 1:1 functional, behavioral, visual, and integration parity with Python Hermes.
A user must not be able to tell the difference between C slermes and Python hermes.

Suite **226/0/23**, 85 tools, 78 commands, 19 gateways, 10 providers, 59 libs.
182 sector gaps (300+ function-level) across 15 sectors (battleship-v11). 85 tools in binary.
Suite 226/0/23. Binary 30MB.

## Phase Order
1. Phase 0b: 5 display/visual gaps (markdown, TUI, banner, progress, errors) (V10 markdown — make it look the same)
2. Phase 0c: 40 CLI commands ignoring args (make commands behave the same)
3. Phase 0d: 15 missing usage patterns (make user experience identical)
4. Phase 1: 4 P1 agent modules (critical backend)
5. Phase 3: 200+ (tool depth) tool/gateway/provider depth (function-level parity)
6. Phase 3: 24 missing ports + plugins
7. Phase 4: 28 library/config/tests

## Loop
Read battleship → pick first gap → implement → build → test → update ALL docs → vault resolved → commit → repeat. No questions, no choices, no stopping.
