# Slermes C — Goal Mantra (v3)

P0: Full 1:1 functional AND visual parity with Python Hermes Agent.
Users must NOT be able to tell the difference between C slermes and Python Hermes.

## The Loop
1. Read battleship → pick highest priority unclosed gap (Phase 0 first)
2. Implement in C — must produce IDENTICAL visual output to Python
3. Build: `make -j$(nproc)` — 0 errors
4. Test: `bash test_runner.sh` — 226/0/23
5. **Visual verify:** compare C output side-by-side with Python Hermes output
6. Mark done → update ALL walkway files → commit
7. Repeat — no questions, no choices

## Current State
- **85 tools, 79 CLI, 19 gateways, 10 providers, 59 libs**
- **226/0/23 test suite, 30MB binary, 427K LOC**
- **~67 real gaps** (~45,000 LOC to port) across 4 phases

## Major Gap Categories
1. **Phase 0 (P0)** — Display & Visual Parity (12 gaps): Skin engine, spinner, banner, status bar, tool feed, response box, help, 256-color, prompt input, markdown, faces, emoji
2. **Phase 1 (P1)** — Critical Agent Modules (4 gaps)
3. **Phase 2 (P2)** — Ports (33 gaps): missing tools, adapters, gateways, agent modules
4. **Phase 3 (P3)** — Depth (18 gaps): smaller modules, tool depth, plugin system

## Rules
- **Visual parity is mandatory.** Every slash command output, every banner, every spinner, every error message must look identical to Python Hermes.
- No stubs. No "for later". No "for brevity". Every gap is real code.
- Every command must produce the same output as Python Hermes
- Test before marking done — visual diff between C and Python output
- After every implementation: update ALL walkway files + battleship + README + banner + all ancillary docs
- Barnacle hunt each stale number across ALL docs with search_files
- Vault resolved gaps to vault/achievements.md with file:line evidence
