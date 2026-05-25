# Slermes C — Goal Mantra

P0: Full 1:1 functional parity with Python Hermes Agent.

## The Loop
1. Read battleship → pick highest priority unclosed gap
2. Implement in C (function, test, wire)
3. Build: `make -j$(nproc)` — 0 errors
4. Test: `bash test_runner.sh` — 226/0/23
5. Verify output matches Python behavior
6. Mark done → update all docs → commit
7. Repeat

## Rules
- No stubs. No "for later". No "for brevity". Every gap is real code.
- Every command must produce the same output as Python Hermes
- Test before marking done — "it compiles" ≠ "it works"
- After every implementation: update state.md AND prestige_prompt AND overnight-map AND battleship
- Barnacle hunt each stale number across ALL docs
- Vault resolved gaps to achievements.md

## Current State
- **85 tools, 79 CLI, 19 gateways, 10 providers, 59 libs**
- **226/0/23 test suite, 30MB binary, 427K LOC**
- Known gaps: ~13 gateway sub-platforms, 7 agent infra modules, ~10 placeholder items
