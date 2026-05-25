# Slermes C — Goal Mantra (v4)

P0: Full 1:1 functional, visual, and integration parity with Python Hermes.

## The Loop
Read battleship-v10.md → pick first gap → implement → build → test → update ALL docs → vault resolved → commit → repeat. No questions, no choices.

## State
300 gaps across 18 sectors (battleship-v10). 85 tools, 79 CLI, 19 gateways, 10 providers, 59 libs. Suite 226/0/23.

## Phase Order
1. Phase 0a: 8 entry point integrations (I01-I08)
2. Phase 0b: 12 display/visual parity (V01-V12)
3. Phase 1: 4 critical agent modules (E01-E04)
4. Phase 2: 193 tool/gateway/provider depth gaps (S2-S5)
5. Phase 3: 24 missing tools/sub-modules/plugins (S6-S8)
6. Phase 4: 28 library/config/test gaps (S9-S11)

## Rules
- No stubs. No "for brevity". No "for later". Every gap is real code.
- Every gap → implement → verify → docs → vault → commit. Never stop at milestones.
- update ALL walkway files after every implementation
- search_files for stale numbers after every patch batch
- Vault resolved gaps with file:line evidence
