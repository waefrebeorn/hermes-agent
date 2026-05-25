# WuBu Hermes — C Translation

> **Zero-dependency single binary Hermes Agent in C**
> 
> Translation of 60K+ lines Python → C. Currently 5,973 lines C across 29 source files.
> **Status: 436 gaps identified** — real working foundation with critical F-N-F fixes needed.
> Binary: 386KB, 0 compile warnings.

## Quick Start

```bash
# Build
make -C C

# Usage
./C/hermes --version
./C/hermes -q "hello"          # One-shot query
./C/hermes                      # Interactive CLI
./C/hermes gateway              # Telegram gateway
./C/hermes cron                 # Cron scheduler
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
│   ├── cron/        ← Scheduler + Jobs (Phase 5) 🟥
│   └── provider/    ← Token exchange, OAuth store 🟧
├── include/         ← 9 headers (hermes.h, hermes_*.h)
├── tests/           ← test_json, test_auth (smoke only)
├── Makefile         ← 5 phase targets
└── .hermes/         ← Battleship, state, goal-mantra
```

## Phase Status (HONEST)

| Phase | Lines | Files | Status | Gaps |
|-------|-------|-------|--------|------|
| 1: Foundation | 2,249 LOC | 6 .c + 6 .h | ✅ Real | 0 critical |
| 2: Agent Core | 861 LOC | 6 .c + 1 .h | 🟧 Partial | 16 F-N-F |
| 3: Tools | 719 LOC | 6 .c + 0 .h | 🟧 Partial | 10 F-N-F |
| 4: Gateway | 351 LOC | 2 .c + 0 .h | 🟧 Partial | 6 F-N-F |
| 5: Cron/Adv | 223 LOC | 2 .c + 0 .h | 🟥 Broken | 5 F-N-F |
| Auth/Provider | 552 LOC | 1 .c + 1 .h | 🟧 Partial | 3 F-N-F |
| **Total** | **5,973 LOC** | **29 .c + 9 .h** | **🟧 Partial** | **67 F-N-F** |

**436 total gaps** — 67 Form-Not-Function (code that compiles but doesn't work), 362 missing features, 1 stub, 6 doc.

## Gap Summary

| Priority | Count | Description |
|----------|-------|-------------|
| 🔴 P0 | 5 | Blocks basic agent function (tool loop, auth, search, cron, docs) |
| 🟠 P1 | 150 | Major features (commands, tools, platforms, tests) |
| 🟡 P2 | 200 | Normal improvements (profiles, display, build system) |
| ⚪ P3 | 81 | Nice-to-haves (Nix, WASM, plugins, advanced comms) |

[Full battleship](C/.hermes/battleship.md) — [HONEST state](C/.hermes/state.md) — [Goal mantra](C/.hermes/goal-mantra.md)

## Verification

```bash
make -C C           # Full build (0 warnings)
./C/hermes --version # WuBu Hermes v0.14.0-wubu
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

- [Battleship — 436 Gap Audit](C/.hermes/battleship.md)
- [HONEST State](C/.hermes/state.md)
- [Goal Mantra](C/.hermes/goal-mantra.md)
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

### Broken
- ❌ Tool calling loop (returns before executing tools)
- ❌ LLM auth header (malformed Content-Type)
- ❌ web_search (alias for GET, not real search)
- ❌ Cron job persistence (memory-only)
- ❌ cron_list_jobs (returns "[]")

### Missing
- ⬜ Memory, compression, profiles, plugins
- ⬜ 38/50+ slash commands
- ⬜ 25+ tool implementations
- ⬜ 17 missing platforms
- ⬜ Multi-provider support
- ⬜ Test infrastructure
- ⬜ CI/CD
- ⬜ MCP/ACP servers