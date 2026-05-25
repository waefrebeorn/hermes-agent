     1|# GOAL MANTRA — Hermes C Translation (HONEST)
     2|
     3|**Mission:** Translate Hermes Agent Python → C for zero-dependency single-binary operation.
     4|
     5|## Current State
     6|- **Binary:** 401KB, compiles with 15 warnings on `make`
     7|- **Source:** 5,411 lines C across 27 `.c` + 9 `.h` files
     8|- **Build:** All 5 phase targets compile (`phase1`–`phase5`/`all`)
     9|- **Gaps:** 392 identified (56 form-not-function, 330 missing, 1 stub, 8 fixed)
    10|- **Target:** Close to ~300 productive gaps, all critical P0 fixed
    11|
    12|## Phase Status (HONEST)
    13|
    14|| Phase | Claimed | Actual | Real Status |
    15||-------|---------|--------|-------------|
    16|| 1: Foundation Deps | ✅ | ✅ | JSON, YAML, HTTP, DB, Crypto, Display wrappers work |
    17|| 2: Agent Core | ✅ | 🟧 | Loop runs with tool execution, multi-turn works. No memory, compression, persistence |
    18|| 3: Tools | ✅ | 🟧 | Only 4 tools registered (need 30+). web_search is an alias. No patch/search |
    19|| 4: Gateway | ✅ | 🟧 | Telegram long-poll works minimally. No other platforms. No approval |
    20|| 5: Cron/Advanced | ✅ | 🟧 | Jobs persist to disk. cron_list works. No skill system |
    21|
    22|## Priority Queue
    23|
    24|### P0 — Fix Now (blocking):
    25|1. ~~Fix tool call loop — agent_loop.c dead code (return before TODO)~~ ✅ DONE
    26|2. ~~Fix auth header — llm_client.c malformed Content-Type~~ ✅ DONE
    27|3. ~~Implement real web_search~~ ✅ DONE (DuckDuckGo Instant Answer API)
    28|4. ~~Implement cron job persistence~~ ✅ DONE (JSON save/load to ~/.hermes/cron_jobs.json)
    29|5. ~~Write HONEST state.md~~ ✅ DONE
    30|
    31|### P1 — Next Wave:
    32|6. Add session persistence (wire db.c into agent loop)
    33|7. Add remaining 38 slash commands
    34|8. Add remaining 25+ tool implementations
    35|9. Add gateway platform abstraction
    36|10. Build test suite
    37|
    38|### P2 — Ongoing:
    39|11. Profile system
    40|12. Credential pool
    41|13. Display spinner/progress/panel
    42|14. Build system improvements (CMake, CI, packaging)
    43|
    44|### P3 — Future:
    45|15. Advanced communication features
    46|16. Static/musl/WASM builds
    47|17. Plugin system
    48|18. MCP/ACP server
    49|
    50|## Rules
    51|- No Python runtime dependency — single binary
    52|- No time estimates — loop until done
    53|- Every F-N-F gap gets fixed before new features
    54|- Tests before code (where practical)
    55|- HONEST docs always — no aspirational claims