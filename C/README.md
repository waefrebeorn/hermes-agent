     1|# WuBu Hermes — C Translation
     2|
     3|> **Zero-dependency single binary Hermes Agent in C**
     4|> 
     5|> Translation of 60K+ lines Python → C. Currently 5,411 lines C across 27 source files.
     6|> **Status: 292 gaps identified** — real working foundation, 8 F-N-F defects fixed.
     7|> Binary: 401KB, 15 pre-existing compile warnings (fortify truncation).
     8|
     9|## Quick Start
    10|
    11|```bash
    12|# Build
    13|make -C C
    14|
    15|# Usage
    16|./C/hermes --version
    17|./C/hermes -q "hello"          # One-shot query
    18|./C/hermes                      # Interactive CLI
    19|./C/hermes gateway              # Telegram gateway
    20|./C/hermes cron                 # Cron scheduler
    21|```
    22|
    23|> ⚠️ **Warning:** This is a work-in-progress C translation. The Python Hermes at project root is the production version. C/ is the parallel porting effort. See battleship for gap status.
    24|
    25|## Architecture
    26|
    27|```
    28|C/
    29|├── src/
    30|│   ├── deps/        ← JSON, YAML, HTTP, DB, Crypto, Display (Phase 1) ✅
    31|│   ├── agent/       ← Loop, LLM client, Context, Title (Phase 2) 🟧
    32|│   ├── cli/         ← CLI, Config, Commands, Display (Phase 2) 🟧
    33|│   ├── tools/       ← Registry, Terminal, File, Web, Skills (Phase 3) 🟧
    34|│   ├── gateway/     ← Server + Telegram (Phase 4) 🟧
    35|│   ├── cron/        ← Scheduler + Jobs (Phase 5) 🟧
    36|│   └── provider/    ← Token exchange, OAuth store 🟧
    37|├── include/         ← 9 headers (hermes.h, hermes_*.h)
    38|├── tests/           ← test_json, test_auth (smoke only)
    39|├── Makefile         ← 5 phase targets
    40|└── .hermes/         ← Battleship, state, goal-mantra
    41|```
    42|
    43|## Phase Status (HONEST)
    44|
    45|| Phase | Lines | Files | Status | F-N-F |
    46||-------|-------|-------|--------|-------|
    47|| 1: Foundation | 2,249 LOC | 6 .c + 6 .h | ✅ Real | 0 |
    48|| 2: Agent Core | 861 LOC | 6 .c + 1 .h | 🟧 Partial | 12 F-N-F |
    49|| 3: Tools | 719 LOC | 6 .c + 0 .h | 🟧 Partial | 8 F-N-F |
    50|| 4: Gateway | 351 LOC | 2 .c + 0 .h | 🟧 Partial | 6 F-N-F |
    51|| 5: Cron/Adv | 337 LOC | 2 .c + 0 .h | 🟧 Partial | 2 F-N-F |
| Auth/Provider | 552 LOC | 1 .c + 1 .h | 🟧 Partial | 3 F-N-F |
| **Total** | **5,411 LOC** | **27 .c + 9 .h** | **🟧 Partial** | **43 F-N-F** |

**292 total gaps** — 43 Form-Not-Function (code that compiles but doesn't work), 198 missing features, 51 partial.
    56|
    57|## Gap Summary
    58|
    59|| Priority | Count | Description |
    60||----------|-------|-------------|
    61|| 🔴 P0 | 5 | All resolved ✅ — tool loop, auth, search, cron, docs |
    62|| 🟠 P1 | 150 | Major features (commands, tools, platforms, tests) |
    63|| 🟡 P2 | 200 | Normal improvements (profiles, display, build system) |
    64|| ⚪ P3 | 37 | Nice-to-haves (plugins, advanced comms) |
    65|
    66|[Full battleship](C/.hermes/battleship.md) — [HONEST state](C/.hermes/state.md) — [Goal mantra](C/.hermes/goal-mantra.md)
    67|
    68|## Verification
    69|
    70|```bash
    71|make -C C           # Full build (15 pre-existing warnings)
    72|./C/hermes --version # WuBu Hermes v0.14.0-wubu
    73|./C/tests/test_json  # JSON tests PASS
    74|./C/tests/test_auth  # Auth tests PASS
    75|```
    76|
    77|## Directory Docs
    78|
    79|| Directory | Contents |
    80||-----------|----------|
    81|| `src/deps/` | JSON, YAML, HTTP, DB, Crypto, Display — standalone wrappers |
    82|| `src/agent/` | Agent loop, LLM client, context, title generation |
    83|| `src/cli/` | CLI orchestrator, config, display, commands, main |
    84|| `src/tools/` | Tool registry + 4 tool implementations |
    85|| `src/gateway/` | Gateway server + Telegram adapter |
    86|| `src/cron/` | Scheduler + job management |
    87|| `src/provider/` | OAuth PKCE token exchange + auth store |
    88|| `include/` | Master header + 8 module-specific headers |
    89|| `tests/` | JSON + Auth smoke tests |
    90|| `.hermes/` | Project management (battleship, state, goal-mantra) |
    91|
    92|## References
    93|
    94|- [Battleship — 292 Gap Audit](C/.hermes/battleship.md)
    95|- [HONEST State](C/.hermes/state.md)
    96|- [Goal Mantra](C/.hermes/goal-mantra.md)
    97|- [DEPENDENCIES.md](C/DEPENDENCIES.md) — Python→C dependency map
    98|- [digestion.md](C/digestion.md) — Update flow docs
    99|
   100|## Appendix: What the Binary Does Today
   101|
   102|### Working
   103|- ✅ `--version` prints banner
   104|- ✅ `-q "query"` one-shot mode (tries LLM call)  
   105|- ✅ Interactive CLI (fgets-based, 4 commands)
   106|- ✅ Config loading (YAML + .env)
   107|- ✅ Message context management
   108|- ✅ JSON parser (parse, serialize, pretty-print, copy)
   109|- ✅ YAML config parser
   110|- ✅ HTTP client (raw socket + OpenSSL)
   111|- ✅ Crypto (SHA-256, HMAC, base64, PKCE)
   112|- ✅ File-based session store
   113|- ✅ OAuth token exchange (PKCE)
   114|- ✅ Auth store (auth.json CRUD)
   115|- ✅ Telegram gateway (basic message/response)
   116|- ✅ Tool calling loop (tools execute and loop back) ✅
   117|- ✅ LLM auth header (Content-Type properly set) ✅
   118|- ✅ web_search (DuckDuckGo Instant Answer API) ✅
   119|- ✅ Cron job persistence (JSON save/load) ✅
   120|- ✅ cron_list_jobs (linked list iteration) ✅
   121|
   122|### Missing
   123|- ⬜ Memory, compression, profiles, plugins
   124|- ⬜ 38/50+ slash commands
   125|- ⬜ 25+ tool implementations
   126|- ⬜ 17 missing platforms
   127|- ⬜ Multi-provider support
   128|- ⬜ Test infrastructure
   129|- ⬜ CI/CD
   130|- ⬜ MCP/ACP servers
   131|