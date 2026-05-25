# WuBu Slermes — C Translation

> **Zero-dependency single binary Slermes AI Agent in C**
> 
> Translation of Python Hermes Agent → C Slermes. Currently 5,411 lines C across 27 source files.
> **Status: 292 gaps identified** — real working foundation, 8 F-N-F defects fixed.
> Binary: 401KB, 15 pre-existing compile warnings (fortify truncation).

## Quick Start

```bash
# Build
make -C C

# Usage
./C/slermes --version
./C/slermes -q "hello"          # One-shot query
./C/slermes                      # Interactive CLI
./C/slermes gateway              # Telegram gateway
./C/slermes cron                 # Cron scheduler
```

> ⚠️ **Warning:** This is a work-in-progress C translation. The Python Hermes at project root is the production version. C/ is the parallel porting effort. See battleship for gap status.

## Architecture

```
C/
├── src/
│   ├── deps/        ← JSON, YAML, HTTP, DB, Crypto, Display (Phase 1) ✅
│   ├── agent/       ← Loop, LLM client, Context, Title (Phase 2) 🟧
│   ├── cli/         ← CLI, Config, Commands, Display (Phase 2) 🟧
│   ├── tools/       ← Registry, Terminal, File, Web, Skills (Phase 3) 🟧
│   ├── gateway/     ← Server + Telegram (Phase 4) 🟧
│   ├── cron/        ← Scheduler + Jobs (Phase 5) 🟧
│   └── provider/    ← Token exchange, OAuth store 🟧
├── include/         ← 9 headers (hermes.h, hermes_*.h)
├── tests/           ← test_json, test_auth (smoke only)
├── Makefile         ← 5 phase targets
└── .hermes/         ← Battleship, state, goal-mantra
```

## Phase Status (HONEST)

| Phase | Lines | Files | Status | F-N-F |
|-------|-------|-------|--------|-------|
| 1: Foundation | 2,249 LOC | 6 .c + 6 .h | ✅ Real | 0 |
| 2: Agent Core | 861 LOC | 6 .c + 1 .h | 🟧 Partial | 12 F-N-F |
| 3: Tools | 719 LOC | 6 .c + 0 .h | 🟧 Partial | 8 F-N-F |
| 4: Gateway | 351 LOC | 2 .c + 0 .h | 🟧 Partial | 6 F-N-F |
| 5: Cron/Adv | 337 LOC | 2 .c + 0 .h | 🟧 Partial | 2 F-N-F |
| Auth/Provider | 552 LOC | 1 .c + 1 .h | 🟧 Partial | 3 F-N-F |
| **Total** | **5,411 LOC** | **27 .c + 9 .h** | **🟧 Partial** | **43 F-N-F** |

**292 total gaps** — 43 Form-Not-Function (code that compiles but doesn't work), 198 missing features, 51 partial.

## Gap Summary

| Priority | Count | Description |
|----------|-------|-------------|
| 🔴 P0 | 5 | All resolved ✅ — tool loop, auth, search, cron, docs |
| 🟠 P1 | 150 | Major features (commands, tools, platforms, tests) |
| 🟡 P2 | 200 | Normal improvements (profiles, display, build system) |
| ⚪ P3 | 37 | Nice-to-haves (plugins, advanced comms) |

[Full battleship](.hermes/mind-palace/battleship-v8.md) — [HONEST state](.hermes/mind-palace/state.md) — [Goal mantra](.hermes/mind-palace/goal-mantra.md)

## Verification

```bash
make -C C           # Full build (15 pre-existing warnings)
./C/slermes --version # WuBu Slermes v0.14.0-wubu
./C/tests/test_json  # JSON tests PASS
./C/tests/test_auth  # Auth tests PASS
```

## Directory Docs

| Directory | Contents |
|-----------|----------|
| `src/deps/` | JSON, YAML, HTTP, DB, Crypto, Display — standalone wrappers |
| `src/agent/` | Agent loop, LLM client, context, title generation |
| `src/cli/` | CLI orchestrator, config, display, commands, main |
| `src/tools/` | Tool registry + 4 tool implementations |
| `src/gateway/` | Gateway server + Telegram adapter |
| `src/cron/` | Scheduler + job management |
| `src/provider/` | OAuth PKCE token exchange + auth store |
| `include/` | Master header + 8 module-specific headers |
| `tests/` | JSON + Auth smoke tests |
| `.hermes/` | Project management (battleship, state, goal-mantra) |

## References

- [Battleship — 292 Gap Audit](.hermes/mind-palace/battleship-v8.md)
- [HONEST State](.hermes/mind-palace/state.md)
- [Goal Mantra](.hermes/mind-palace/goal-mantra.md)
- [DEPENDENCIES.md](C/DEPENDENCIES.md) — Python→C dependency map
- [digestion.md](C/digestion.md) — Update flow docs

## Appendix: What the Binary Does Today

### Working
- ✅ `--version` prints banner
- ✅ `-q "query"` one-shot mode (tries LLM call)  
- ✅ Interactive CLI (fgets-based, 4 commands)
- ✅ Config loading (YAML + .env)
- ✅ Message context management
- ✅ JSON parser (parse, serialize, pretty-print, copy)
- ✅ YAML config parser
- ✅ HTTP client (raw socket + OpenSSL)
- ✅ Crypto (SHA-256, HMAC, base64, PKCE)
- ✅ File-based session store
- ✅ OAuth token exchange (PKCE)
- ✅ Auth store (auth.json CRUD)
- ✅ Telegram gateway (basic message/response)
- ✅ Tool calling loop (tools execute and loop back) ✅
- ✅ LLM auth header (Content-Type properly set) ✅
- ✅ web_search (DuckDuckGo Instant Answer API) ✅
- ✅ Cron job persistence (JSON save/load) ✅
- ✅ cron_list_jobs (linked list iteration) ✅

### Missing
- ⬜ Memory, compression, profiles, plugins
- ⬜ 38/50+ slash commands
- ⬜ 25+ tool implementations
- ⬜ 17 missing platforms
- ⬜ Multi-provider support
- ⬜ Test infrastructure
- ⬜ CI/CD
- ⬜ MCP/ACP servers
