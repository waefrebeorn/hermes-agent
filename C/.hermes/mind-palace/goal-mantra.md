     1|# Slermes C — Goal Mantra (v9)
     2|
     3|P0: Full 1:1 functional, behavioral, visual, and integration parity with Python Hermes.
     4|A user must not be able to tell the difference between C slermes and Python hermes.
     5|
     6|## State
     7|349 gaps across 20 sectors (battleship-v10). 85 tools, 79 CLI, 19 gateways.
     8|Suite 226/0/23. Binary 30MB.
     9|
    10|## Phase Order
    11|    12|1. Phase 0b: 9 display/visual parity (make it look the same)
    13|2. Phase 0c: 40 CLI commands ignoring args (make commands behave the same)
    14|3. Phase 0d: 15 missing usage patterns (make user experience identical)
    15|4. Phase 1: 4 P1 agent modules (critical backend)
    16|5. Phase 2: 193 tool/gateway/provider depth (function-level parity)
    17|6. Phase 3: 24 missing ports + plugins
    18|7. Phase 4: 28 library/config/tests
    19|
    20|## Loop
    21|Read battleship → pick first gap → implement → build → test → update ALL docs → vault resolved → commit → repeat. No questions, no choices, no stopping.
    22|
