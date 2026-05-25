# Slermes C — Plan (v4 — 300 Gaps)

## Verified State
Suite 226/0/23. Binary 30MB. 85 tools, 79 CLI, 19 gateways, 10 providers, 59 libs.

## Gap Count: 300 across 18 sectors (battleship-v10)
- Phase 0a: 8 entry point fixes
- Phase 0b: 12 display/visual parity
- Phase 1: 4 critical agent modules
- Phase 2: 193 tool depth + gateway + provider + agent depth
- Phase 3: 24 missing tools + sub-modules + plugins
- Phase 4: 28 library + config + test coverage

## First Actions
1. I01: Fix pipe mode multi-line stdin dispatch (cli.c:548-588)
2. I02: Reject unknown flags (main.c)
3. I03: Wire --tui flag (main.c)
4. I04: Validate --session arg (main.c)
5. I05: Fix log path to slermes logs (commands.c)

## Vault
- achievements.md: All resolved gaps and stale claims
- hermes-upstream.md: Python modules not ported
