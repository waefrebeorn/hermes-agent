     1|# Slermes C — Plan (v8 — 348 Gaps)
     2|
     3|## Verified State
     4|Suite 226/0/23. Binary 30MB. 85 tools, 79 CLI, 19 gateways, 10 providers, 59 libs.
     5|
     6|## Gap Count: 348 across 20 sectors (battleship-v10)
     7|     8|- Phase 0b: 9 display/visual parity
     9|- Phase 1: 4 critical agent modules
    10|- Phase 2: 193 tool depth + gateway + provider + agent depth
    11|- Phase 3: 24 missing tools + sub-modules + plugins
    12|- Phase 4: 28 library + config + test coverage
    13|
    14|## First Actions
    15|1. I01: Fix pipe mode multi-line stdin dispatch (cli.c:548-588)
    16|2. I02: Reject unknown flags (main.c)
    17|3. I03: Wire --tui flag (main.c)
    18|4. I04: Validate --session arg (main.c)
    19|5. I05: Fix log path to slermes logs (commands.c)
    20|
    21|## Vault
    22|- achievements.md: All resolved gaps and stale claims
    23|- hermes-upstream.md: Python modules not ported
    24|
